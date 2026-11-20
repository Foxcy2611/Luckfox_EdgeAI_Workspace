#!/usr/bin/env python3
"""Large-window, latest-frame LuckFox preview (FVP2 TCP, not RTSP).
Inference runs on the board. The GUI renders fresh frames only.

Run on Ubuntu: python3 Deploy/view_face_verification.py
Keys: q / Escape to quit, f to toggle fullscreen.
"""
import argparse
import queue
import socket
import threading
import time

import cv2
import numpy as np


def recv_stream(host: str, port: int, frames: queue.Queue, stop: threading.Event) -> None:
    while not stop.is_set():
        try:
            with socket.create_connection((host, port), timeout=3.0) as sock:
                sock.settimeout(3.0)
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                print(f"[PREVIEW] Connected to {host}:{port}", flush=True)
                with sock.makefile("rb") as stream:
                    while not stop.is_set():
                        line = stream.readline(4096)
                        if not line:
                            raise ConnectionError("Board disconnected")
                        cols = line.decode("ascii", errors="replace").strip().split()
                        if len(cols) < 5 or cols[0] != "FVP2":
                            raise ValueError("Unexpected preview header")
                        width, height, index, nfaces = map(int, cols[1:5])
                        if not (0 < width <= 1920 and 0 < height <= 1080 and 0 <= nfaces <= 8):
                            raise ValueError("Invalid frame dimensions or face count")
                        if len(cols) != 5 + nfaces * 17:
                            raise ValueError("Invalid face metadata field count")
                        payload = stream.read(width * height * 3)
                        if len(payload) != width * height * 3:
                            raise ConnectionError("Truncated RGB packet")
                        faces = [list(map(float, cols[5 + i * 17:5 + (i + 1) * 17]))
                                 for i in range(nfaces)]
                        item = (width, height, index, faces, payload)
                        # Keep at most ONE complete frame; discard stale UI frames.
                        try:
                            frames.put_nowait(item)
                        except queue.Full:
                            try:
                                frames.get_nowait()
                            except queue.Empty:
                                pass
                            frames.put_nowait(item)
        except (OSError, ValueError, ConnectionError) as exc:
            if not stop.is_set():
                print(f"[PREVIEW] {exc}; reconnecting...", flush=True)
                stop.wait(0.4)


def render(item, width_out: int, fps: float):
    width, height, index, faces, payload = item
    raw = np.frombuffer(payload, np.uint8).reshape(height, width, 3)
    frame = cv2.cvtColor(raw, cv2.COLOR_RGB2BGR)
    height_out = max(1, round(height * width_out / width))
    frame = cv2.resize(frame, (width_out, height_out), interpolation=cv2.INTER_LINEAR)
    sx, sy = width_out / width, height_out / height

    for info in faces:
        x, y, w, h, confidence, similarity, verified = info[:7]
        landmarks = info[7:17]
        color = ((255, 200, 0) if verified > 1.5 else
                 ((0, 220, 0) if verified > 0.5 else (0, 100, 255)))
        left, top = round(x * sx), round(y * sy)
        right, bottom = round((x + w) * sx), round((y + h) * sy)
        cv2.rectangle(frame, (left, top), (right, bottom), color, 3)
        for i in range(5):
            cv2.circle(frame, (round(landmarks[i * 2] * sx),
                               round(landmarks[i * 2 + 1] * sy)),
                       4, (255, 255, 0), -1)
        status = ("DETECTED" if verified > 1.5 else
                  "VERIFIED" if verified > 0.5 else "UNKNOWN")
        label = (f"{status} det={confidence:.2f}" if verified > 1.5 else
                 f"{status} det={confidence:.2f} cos={similarity:.3f}")
        cv2.putText(frame, label, (max(0, left), max(32, top - 10)),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.85, color, 2, cv2.LINE_AA)

    cv2.rectangle(frame, (0, 0), (min(width_out, 650), 48), (0, 0, 0), -1)
    cv2.putText(frame, f"LuckFox NPU | Frame {index} | Viewer {fps:.1f} fps",
                (12, 32), cv2.FONT_HERSHEY_SIMPLEX, 0.85, (255, 255, 255), 2,
                cv2.LINE_AA)
    return frame


def main():
    parser = argparse.ArgumentParser(description="Large realtime LuckFox Face Verification viewer")
    parser.add_argument("--host", default="172.32.0.93")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--window-width", type=int, default=1280,
                        help="Rendered preview width (default 1280); height auto follows aspect ratio")
    parser.add_argument("--fullscreen", action="store_true")
    options = parser.parse_args()
    if not 320 <= options.window_width <= 3840:
        parser.error("--window-width must be in [320, 3840]")

    frames = queue.Queue(maxsize=1)
    stop = threading.Event()
    worker = threading.Thread(target=recv_stream,
                              args=(options.host, options.port, frames, stop), daemon=True)
    worker.start()

    title = "LuckFox Pico Max | Face Verification"
    cv2.namedWindow(title, cv2.WINDOW_NORMAL)
    cv2.resizeWindow(title, options.window_width, round(options.window_width * 3 / 4))
    fullscreen = options.fullscreen
    if fullscreen:
        cv2.setWindowProperty(title, cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)

    fps, prev_time = 0.0, None
    print("[VIEW] q/Escape: exit | f: toggle fullscreen", flush=True)
    try:
        while True:
            try:
                item = frames.get(timeout=0.04)
                while True:  # newest available packet only
                    try:
                        item = frames.get_nowait()
                    except queue.Empty:
                        break
                now = time.monotonic()
                if prev_time is not None and now > prev_time:
                    instant = 1.0 / (now - prev_time)
                    fps = instant if not fps else fps * 0.85 + instant * 0.15
                prev_time = now
                cv2.imshow(title, render(item, options.window_width, fps))
            except queue.Empty:
                pass
            key = cv2.waitKey(1) & 0xFF
            if key in (27, ord("q")):
                break
            if key == ord("f"):
                fullscreen = not fullscreen
                cv2.setWindowProperty(title, cv2.WND_PROP_FULLSCREEN,
                                      cv2.WINDOW_FULLSCREEN if fullscreen else cv2.WINDOW_NORMAL)
                if not fullscreen:
                    cv2.resizeWindow(title, options.window_width,
                                     round(options.window_width * 3 / 4))
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
