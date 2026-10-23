// Display PostScript inustroke replacement; coordinates stay in world space.
#include "wraps.h"
#include "pathops.h"
namespace de {
bool hitPath(Vec2 p, const std::vector<Vec2> &path, float tolerance) {
    for (size_t i = 1; i < path.size(); ++i)
        if (distanceToLine(p, path[i - 1], path[i]) <= tolerance)
            return true;
    return false;
}
} // namespace de
