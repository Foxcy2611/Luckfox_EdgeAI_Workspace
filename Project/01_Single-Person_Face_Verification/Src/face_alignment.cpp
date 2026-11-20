#include "face_alignment.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

// 5-point similarity warp to the same 112x112 template as OpenCV SFace.
// The input RGB frame is warped using inverse mapping and bilinear interpolation.
Image align_face_sface(const Image& src,const FaceDetection& face) {
    if(!src.valid()) throw std::runtime_error("SFace align: invalid image");
    constexpr double dst[5][2]={{38.2946,51.6963},{73.5318,51.5014},
                                {56.0252,71.7366},{41.5493,92.3655},{70.7299,92.2041}};
    double mx=0,my=0,dx=0,dy=0;
    for(int i=0;i<5;i++) {mx+=face.landmarks[i].x;my+=face.landmarks[i].y;dx+=dst[i][0];dy+=dst[i][1];}
    mx/=5;my/=5;dx/=5;dy/=5;
    double a=0,b=0,den=0;
    for(int i=0;i<5;i++) {
        const double x=face.landmarks[i].x-mx, y=face.landmarks[i].y-my;
        const double u=dst[i][0]-dx,v=dst[i][1]-dy;
        a+=x*u+y*v; b+=x*v-y*u; den+=x*x+y*y;
    }
    if(den<1e-4) throw std::runtime_error("Face landmarks too close together");
    a/=den; b/=den;
    const double tx=dx-a*mx+b*my,ty=dy-b*mx-a*my;
    const double det=a*a+b*b;
    if(det<1e-7) throw std::runtime_error("Face alignment degenerate");
    Image out;out.width=112;out.height=112;out.rgb.assign(112*112*3,0);
    for(int y=0;y<112;y++) for(int x=0;x<112;x++) {
        // inverse of [a -b tx; b a ty]
        const double sx=(a*(x-tx)+b*(y-ty))/det;
        const double sy=(-b*(x-tx)+a*(y-ty))/det;
        if(sx<0 || sy<0 || sx>src.width-1 || sy>src.height-1) continue;
        const int x0=static_cast<int>(std::floor(sx)),y0=static_cast<int>(std::floor(sy));
        const int x1=std::min(src.width-1,x0+1),y1=std::min(src.height-1,y0+1);
        const double fx=sx-x0,fy=sy-y0;
        for(int c=0;c<3;c++) {
            auto at=[&](int xx,int yy) {return src.rgb[(static_cast<std::size_t>(yy)*src.width+xx)*3+c];};
            double pixel=(1-fy)*((1-fx)*at(x0,y0)+fx*at(x1,y0))+
                                fy*((1-fx)*at(x0,y1)+fx*at(x1,y1));
            out.rgb[(static_cast<std::size_t>(y)*112+x)*3+c]=static_cast<std::uint8_t>(std::lround(pixel));
        }
    }
    return out;
}
