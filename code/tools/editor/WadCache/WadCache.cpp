#include "WadCache.h"
#include "../Viewport3D.h"
namespace de {
ImTextureID App::image(const std::string &name, const std::function<Image()> &make) {
    auto found = imageCache.find(name);
    if (found != imageCache.end())
        return found->second;
    try {
        auto id = uploadImage(make());
        imageCache[name] = id;
        return id;
    } catch (const std::exception &e) {
        message(name + ": " + e.what());
        imageCache[name] = 0;
        return 0;
    }
}
void App::clearImages() {
    invalidate3DTextures();
    for (auto [name, id] : imageCache)
        if (id)
            retiredImages.push_back(id);
    imageCache.clear();
}
} // namespace de
