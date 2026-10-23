// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../qcommon/doom_display.h"
#include <cstdio>
#include <limits>
int main() {
    using namespace doomdisplay;
    for (auto &mode:modes) if (!IsWidescreen(mode.width,mode.height)) return 1;
    if (IsWidescreen(640,480) || IsWidescreen(1920,1200) || IsWidescreen(0,0) || IsWidescreen(2147483647,2147483647)) return 2;
    float x,y;
    Fov(90,640,480,x,y);
    if (std::abs(x-90)>.001 || std::abs(y-73.739795f)>.001) return 3;
    Fov(90,1920,1080,x,y);
    if (std::abs(x-106.260205f)>.001 || std::abs(y-73.739795f)>.001) return 4;
    Fov(30,1920,1080,x,y);
    if (x<=30 || x>=40 || y<=0 || y>=30) return 5;
    Fov(160,7680,4320,x,y);
    if (!std::isfinite(x) || x>=180 || y>=180) return 6;
    Fov(std::numeric_limits<float>::quiet_NaN(),0,0,x,y);
    if (!std::isfinite(x) || !std::isfinite(y)) return 7;
    float firstX=x,firstY=y;
    Fov(90,1280,720,x,y);
    if (std::abs(x-firstX)>.001 || std::abs(y-firstY)>.001) return 8;
    Fov(90,1920,908,x,y,1920,1080);
    if (std::abs(x-106.260205f)>.001 || y>=73.739795f || y<=60) return 9;
    puts("Display checks passed: only 16:9 modes/custom sizes, original reference FOV, Hor+ widescreen, zoom, limits, invalid input.");
    return 0;
}
