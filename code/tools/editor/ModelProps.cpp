#include "ModelProps.h"
#include "NodeBuild.h"
#include <windows.h>
namespace de {
namespace {
doom_models::Registry registry;
bool initialized = false;
} // namespace
void reloadModelProps() {
    registry.discover(projectDirectory() / "base/editor/models");
    initialized = true;
}
doom_models::Registry &modelProps() {
    if (!initialized)
        reloadModelProps();
    return registry;
}
} // namespace de
