#include <windows.h>
#include "UI.h"
#include "idfunctions.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace de {
namespace {
Image readReferenceImage(const std::filesystem::path &path) {
    using Microsoft::WRL::ComPtr;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct Apartment {
        bool owned;
        ~Apartment() {
            if (owned)
                CoUninitialize();
        }
    } apartment{SUCCEEDED(initialized)};
    auto check = [&](HRESULT result) {
        require(SUCCEEDED(result), "Cannot open reference image: " + path.u8string());
    };
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&factory)));
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                             WICDecodeMetadataCacheOnLoad, &decoder));
    ComPtr<IWICBitmapFrameDecode> frame;
    check(decoder->GetFrame(0, &frame));
    UINT width = 0, height = 0;
    check(frame->GetSize(&width, &height));
    require(width && height && width <= 16384 && height <= 16384 &&
                uint64_t(width) * height <= 64 * 1024 * 1024,
            "Reference image is too large (maximum 16384 per side and 64 million pixels)");
    ComPtr<IWICFormatConverter> converter;
    check(factory->CreateFormatConverter(&converter));
    check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                nullptr, 0, WICBitmapPaletteTypeCustom));
    Image image{int(width), int(height), std::vector<uint32_t>(size_t(width) * height)};
    check(converter->CopyPixels(nullptr, width * 4, width * height * 4,
                                reinterpret_cast<BYTE *>(image.pixels.data())));
    return image;
}

void fitReferenceImage(App &a) {
    auto &r = a.referenceImage;
    r.center = a.view.center;
    r.rotation = 0;
    const float width = std::max(1.f, a.view.size.x), height = std::max(1.f, a.view.size.y);
    r.scale =
        std::clamp(.8f * std::min(width / r.width, height / r.height) / a.view.zoom, .001f, 1024.f);
}
} // namespace

void App::loadReferenceImage(const std::filesystem::path &path) {
    const auto image = readReferenceImage(path);
    ReferenceImage next;
    next.path = std::filesystem::absolute(path).lexically_normal();
    next.width = image.width;
    next.height = image.height;
    // Decode and upload before replacing the current reference, so failures retain it.
    next.texture = uploadImage(image);
    world.checkpoint();
    removeReferenceImage(false);
    referenceImage = std::move(next);
    fitReferenceImage(*this);
    const auto &r = referenceImage;
    world.reference = {r.path.u8string(), r.center, r.scale, r.opacity, r.rotation};
}

void App::removeReferenceImage(bool edit) {
    if (edit && !world.reference.path.empty()) {
        world.checkpoint();
        world.reference = {};
    }
    if (referenceImage.texture)
        retiredImages.push_back(referenceImage.texture);
    referenceImage = {};
}

void App::syncReferenceImage() {
    const auto &saved = world.reference;
    auto path = std::filesystem::u8path(saved.path);
    if (!path.empty() && path.is_relative())
        path = std::filesystem::absolute(world.pathname).parent_path() / path;
    if (referenceImage.path != path) {
        removeReferenceImage(false);
        referenceImage.path = path;
        if (!path.empty()) {
            try {
                auto image = readReferenceImage(path);
                referenceImage.texture = uploadImage(image);
                referenceImage.width = image.width;
                referenceImage.height = image.height;
            } catch (const std::exception &e) {
                message("Reference image unavailable: " + path.u8string() +
                        "; use Replace image to relink. " + e.what());
            }
        }
    }
    referenceImage.center = saved.center;
    referenceImage.scale = saved.scale;
    referenceImage.opacity = saved.opacity;
    referenceImage.rotation = saved.rotation;
}

void drawReferenceImage(const App &a, ImDrawList *draw) {
    const auto &r = a.referenceImage;
    if (!r.texture || r.opacity <= 0)
        return;
    const float angle = r.rotation * 3.14159265359f / 180.f;
    const float c = std::cos(angle), s = std::sin(angle);
    const float halfWidth = r.width * r.scale * .5f, halfHeight = r.height * r.scale * .5f;
    auto corner = [&](float x, float y) {
        return worldToScreen(a, {r.center.x + x * c - y * s, r.center.y + x * s + y * c});
    };
    const auto tint = ImGui::ColorConvertFloat4ToU32({1, 1, 1, std::clamp(r.opacity, 0.f, 1.f)});
    draw->AddImageQuad(r.texture, corner(-halfWidth, halfHeight), corner(halfWidth, halfHeight),
                       corner(halfWidth, -halfHeight), corner(-halfWidth, -halfHeight), {0, 0},
                       {1, 0}, {1, 1}, {0, 1}, tint);
}

void drawReferenceImageControls(App &a) {
    if (ImGui::Button("Reference..."))
        ImGui::OpenPopup("Background reference");
    ImGui::SetNextWindowSizeConstraints({340 * a.dpi, 0}, {500 * a.dpi, FLT_MAX});
    if (!ImGui::BeginPopup("Background reference"))
        return;
    ImGui::TextUnformatted("2D background reference");
    if (ImGui::Button(!a.referenceImage.path.empty() ? "Replace image..." : "Load image..."))
        a.action([&] {
            const auto path = fileDialog(false,
                                         L"Images "
                                         L"(*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff)\0*.png;*.jpg;*."
                                         L"jpeg;*.bmp;*.tif;*.tiff\0All files\0*.*\0",
                                         L"png");
            if (path.empty())
                return;
            a.loadReferenceImage(path);
            a.message("Loaded reference image: " + path.filename().u8string());
        });
    auto &r = a.referenceImage;
    if (!r.path.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Remove image")) {
            a.removeReferenceImage();
            a.message("Removed reference image");
        }
    }
    if (!r.path.empty()) {
        ImGui::TextWrapped("%s (%d x %d)", r.path.filename().u8string().c_str(), r.width, r.height);
        if (!r.texture)
            ImGui::TextWrapped(
                "Image missing. Replace image to relink; saved transforms are retained.");
        ImGui::Separator();
        const auto before = a.world.reference;
        ImGui::PushItemWidth(180 * a.dpi);
        ImGui::SliderFloat("Opacity", &r.opacity, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        float scale = r.scale;
        if (ImGui::InputFloat("Units per pixel", &scale, .1f, 1.f, "%.3f") && std::isfinite(scale))
            r.scale = std::clamp(scale, .001f, 1024.f);
        float center[2]{r.center.x, r.center.y};
        if (ImGui::InputFloat2("Center X / Y", center, "%.1f") && std::isfinite(center[0]) &&
            std::isfinite(center[1]))
            r.center = {std::clamp(center[0], -1000000.f, 1000000.f),
                        std::clamp(center[1], -1000000.f, 1000000.f)};
        float rotation = r.rotation;
        if (ImGui::InputFloat("Rotation (deg)", &rotation, 1.f, 15.f, "%.1f") &&
            std::isfinite(rotation))
            r.rotation = std::fmod(rotation, 360.f);
        ImGui::PopItemWidth();
        ImGui::Text("Map size: %.1f x %.1f units", r.width * r.scale, r.height * r.scale);
        if (ImGui::Button("Center in view"))
            r.center = a.view.center;
        ImGui::SameLine();
        if (ImGui::Button("Fit to view") && r.width && r.height)
            fitReferenceImage(a);
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            r.scale = 1;
            r.rotation = 0;
            r.opacity = .5f;
            r.center = a.view.center;
        }
        ReferenceSettings after{r.path.u8string(), r.center, r.scale, r.opacity, r.rotation};
        if (!(before == after)) {
            a.world.checkpoint();
            a.world.reference = std::move(after);
        }
        ImGui::TextWrapped(
            "Scale 2 makes the image twice as large. Opacity 0 hides it completely.");
    } else {
        ImGui::TextWrapped(
            "Load a map screenshot or other image to trace beneath the grid and geometry.");
    }
    ImGui::TextWrapped(
        "Image path and transforms are saved in the DWD. The image is not exported to the game.");
    ImGui::EndPopup();
}
} // namespace de
