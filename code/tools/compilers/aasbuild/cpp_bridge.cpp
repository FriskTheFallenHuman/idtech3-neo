// Shared engine APIs used by BSPC with the C++ signatures from qcommon.h.
#include "../../../game/game/q_shared.h"
#include "../../../qcommon/qcommon.h"
void *Hunk_Alloc(int size);
void *Hunk_Alloc(int size, ha_pref) { return Hunk_Alloc(size); }

int BoxOnPlaneSide(vec3_t mins, vec3_t maxs, cplane_t *plane) {
    float nearDistance = 0, farDistance = 0;
    for (int i = 0; i < 3; ++i) {
        if (plane->normal[i] >= 0) {
            nearDistance += plane->normal[i] * mins[i];
            farDistance += plane->normal[i] * maxs[i];
        } else {
            nearDistance += plane->normal[i] * maxs[i];
            farDistance += plane->normal[i] * mins[i];
        }
    }
    return (farDistance >= plane->dist ? 1 : 0) | (nearDistance < plane->dist ? 2 : 0);
}

void CreateRotationMatrix(vec3_t angles, float matrix[3][3]) {
    AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
    VectorInverse(matrix[1]);
}

