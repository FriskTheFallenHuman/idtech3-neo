// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>

namespace doomdisplay {
struct Mode { const char *description; int width, height; float pixelAspect; };
inline constexpr Mode modes[] = {
    {"Mode  0: 640x360 (16:9)",640,360,1},
    {"Mode  1: 960x540 (16:9)",960,540,1},
    {"Mode  2: 1280x720 (16:9)",1280,720,1},
    {"Mode  3: 1600x900 (16:9)",1600,900,1},
    {"Mode  4: 1920x1080 (16:9)",1920,1080,1},
    {"Mode  5: 2560x1440 (16:9)",2560,1440,1},
    {"Mode  6: 3200x1800 (16:9)",3200,1800,1},
    {"Mode  7: 3840x2160 (16:9)",3840,2160,1},
    {"Mode  8: 5120x2880 (16:9)",5120,2880,1},
    {"Mode  9: 7680x4320 (16:9)",7680,4320,1}
};
inline bool IsWidescreen(int width,int height) {
    return width>=320 && height>=180 && width<=16384 && height<=9216 && width*9==height*16;
}
// cg_fov is horizontal at the original 4:3 reference aspect. Keep vertical
// coverage while revealing more of the world to each side on widescreen.
inline void Fov(float base,int width,int height,float &horizontal,float &vertical,int displayWidth=0,int displayHeight=0) {
    constexpr double pi=3.14159265358979323846;
    if (!std::isfinite(base)) base=90;
    if (base<1) base=1;
    if (base>160) base=160;
    double tangent=std::tan(base*pi/360.)*.75;
    double viewportAspect=width>0 && height>0 ? double(width)/height : 16./9.;
    double aspect=displayWidth>0 && displayHeight>0 ? double(displayWidth)/displayHeight : viewportAspect;
    horizontal=float(std::atan(tangent*aspect)*360./pi);
    // The status bar and letterboxing crop vertically without distorting pixels.
    vertical=float(std::atan(tangent*aspect/viewportAspect)*360./pi);
}
}
