#pragma once
#include "R_mapdef.h"
namespace de {
struct SplineSample {
    Vec2 point;
    double distance = 0;
    int segment = 0;
};
std::vector<SplineSample> sampleThingSpline(const ThingSpline &);
Vec2 splinePointAt(const std::vector<SplineSample> &, double distance, bool closed = false);
std::vector<Thing> generateSplineThings(const ThingSpline &);
void validateThingSpline(const ThingSpline &);
} // namespace de
