#pragma once

// Native interface shared by the engine and spline implementation.
extern "C" {
int loadCamera(int camNum, const char *name);
void startCamera(int camNum, int time);
int getCameraInfo(int camNum, int time, float *origin, float *angles, float *fov);
}
