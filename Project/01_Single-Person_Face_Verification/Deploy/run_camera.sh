#!/bin/sh
# Run on LuckFox Pico Max after SCP. POSIX /bin/sh for BusyBox/Buildroot.
set -eu

# Deployed under /root/FaceVerification/ with Models/ and Embeddings/.
# Resolve paths relative to this script, independently from the SSH working directory.
WORKDIR=$(CDPATH= cd "$(dirname "$0")" && pwd)
cd "$WORKDIR"

WIDTH=${WIDTH:-640}
HEIGHT=${HEIGHT:-480}
CHANNEL=${CHANNEL:-0}
IQ_DIR=${IQ_DIR:-/etc/iqfiles}
DETECT_THRESHOLD=${DETECT_THRESHOLD:-0.7}
MATCH_THRESHOLD=${MATCH_THRESHOLD:-0.363}
PREVIEW_PORT=${PREVIEW_PORT:-8765}
PREVIEW_EVERY=${PREVIEW_EVERY:-3}
MAX_FRAMES=${MAX_FRAMES:-0}
STOP_RKIPC=1
DETECT_ONLY=0
INSPECT=0

usage() {
    cat <<'EOF'
Usage: sh ./run_camera.sh [options]
  (no options)           FULL: SC3336 + YuNet + SFace + TCP preview
  --stop-rkipc           Same as default: stop rkipc (stops original RTSP)
  --keep-rkipc           Do not stop rkipc; fail if camera is occupied
  --full                 FULL pipeline (default): YuNet + SFace + verification
  --detect-only          Only YuNet; do not load SFace or reference
  --inspect-models       Load models and print tensors; do not open camera
  --no-preview           Disable TCP preview (terminal logs only)
  --max-frames N         Stop after N frames (0 = unlimited)
  --width N --height N   Camera resolution (default 640x480)
  --channel N            RK_MPI channel (default 0)
  --preview-port N       TCP viewer port (default 8765)
  --preview-every N      Send one frame every N processed frames (default 3)
  --detect-threshold X   YuNet score threshold (default 0.7)
  --match-threshold X    Cosine threshold (default 0.363; tune for RKNN)

Examples:
  sh ./run_camera.sh --full
  sh ./run_camera.sh --detect-only
  sh ./run_camera.sh --inspect-models
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --stop-rkipc) STOP_RKIPC=1; shift ;;
        --keep-rkipc) STOP_RKIPC=0; shift ;;
        --detect-only) DETECT_ONLY=1; shift ;;
        --full) DETECT_ONLY=0; shift ;;
        --inspect-models) INSPECT=1; shift ;;
        --no-preview) PREVIEW_PORT=0; shift ;;
        --width|--height|--channel|--preview-port|--preview-every|--max-frames|--detect-threshold|--match-threshold)
            key=$1
            [ "$#" -ge 2 ] || { echo "Missing value for $key" >&2; exit 2; }
            case "$key" in
                --width) WIDTH=$2 ;;
                --height) HEIGHT=$2 ;;
                --channel) CHANNEL=$2 ;;
                --preview-port) PREVIEW_PORT=$2 ;;
                --preview-every) PREVIEW_EVERY=$2 ;;
                --max-frames) MAX_FRAMES=$2 ;;
                --detect-threshold) DETECT_THRESHOLD=$2 ;;
                --match-threshold) MATCH_THRESHOLD=$2 ;;
            esac
            shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

[ -s ./FaceVerification ] || { echo "Missing executable: $WORKDIR/FaceVerification" >&2; exit 1; }
[ -s ./Models/detectors/yunet.rknn ] || { echo "Missing YuNet RKNN model" >&2; exit 1; }
if [ "$DETECT_ONLY" -eq 0 ]; then
    [ -s ./Models/recognizers/sface.rknn ] || { echo "Missing SFace RKNN model" >&2; exit 1; }
    [ -s ./Embeddings/person_a.bin ] || { echo "Missing person_a.bin" >&2; exit 1; }
fi

# Some Buildroot firmwares put the RKNN runtime under /oem/usr/lib.
if [ -d /oem/usr/lib ]; then
    export LD_LIBRARY_PATH="/oem/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

if [ "$DETECT_ONLY" -eq 1 ]; then
    echo "[MODE] DETECTION ONLY: YuNet RKNN (SFace disabled)"
else
    echo "[MODE] FULL: YuNet RKNN + SFace RKNN + cosine verification"
    echo "[MODE] SFace runs ONLY when YuNet detects a face (faces > 0)."
fi

if [ "$INSPECT" -eq 1 ]; then
    echo '[RUN] Inspecting RKNN tensor metadata without opening camera'
    if [ "$DETECT_ONLY" -eq 1 ]; then
        exec ./FaceVerification --inspect-models --detect-only
    fi
    exec ./FaceVerification --inspect-models
fi

if command -v pidof >/dev/null 2>&1 && pidof rkipc >/dev/null 2>&1; then
    if [ "$STOP_RKIPC" -eq 1 ]; then
        echo '[CAM] rkipc is running; stopping it to release SC3336 (existing RTSP will stop).'
        if command -v killall >/dev/null 2>&1; then
            killall rkipc || { echo 'Cannot stop rkipc (check permissions)' >&2; exit 1; }
        else
            echo 'killall not found; stop rkipc manually then retry.' >&2
            exit 1
        fi
        sleep 2
        if pidof rkipc >/dev/null 2>&1; then
            echo 'rkipc is still running; stop its supervisor/service first.' >&2
            exit 1
        fi
    else
        echo '[CAM] rkipc is using the camera. Retry with --stop-rkipc (this stops its RTSP stream).' >&2
        exit 1
    fi
fi

if [ ! -d "$IQ_DIR" ]; then
    echo "[WARN] ISP IQ directory not found: $IQ_DIR (override with IQ_DIR=/path)" >&2
fi

echo "[RUN] SC3336 | ${WIDTH}x${HEIGHT} | channel=${CHANNEL} | preview TCP port=${PREVIEW_PORT}"
if [ "$DETECT_ONLY" -eq 1 ]; then
    exec ./FaceVerification --camera \
        --width "$WIDTH" --height "$HEIGHT" --channel "$CHANNEL" --iq-dir "$IQ_DIR" \
        --detect-only --detect-threshold "$DETECT_THRESHOLD" \
        --preview-port "$PREVIEW_PORT" --preview-every "$PREVIEW_EVERY" \
        --max-frames "$MAX_FRAMES"
fi
exec ./FaceVerification --camera \
    --width "$WIDTH" --height "$HEIGHT" --channel "$CHANNEL" --iq-dir "$IQ_DIR" \
    --detect-threshold "$DETECT_THRESHOLD" --match-threshold "$MATCH_THRESHOLD" \
    --preview-port "$PREVIEW_PORT" --preview-every "$PREVIEW_EVERY" \
    --max-frames "$MAX_FRAMES"