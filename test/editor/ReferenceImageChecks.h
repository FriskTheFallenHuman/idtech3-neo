#pragma once
#include "../UI.h"
#include "../idfunctions.h"
#include "imgui_internal.h"
#include <cmath>
#include <cstring>
#include <memory>

namespace de {
// Called with a live D3D device and ImGui frame, so this exercises WIC, texture
// replacement, and the same map draw list consumed by the renderer.
inline void testReferenceImage(App &a, const std::filesystem::path &fixture) {
    Bytes bitmap;
    auto word = [&](uint16_t v) {
        bitmap.push_back(uint8_t(v));
        bitmap.push_back(uint8_t(v >> 8));
    };
    auto integer = [&](uint32_t v) {
        word(uint16_t(v));
        word(uint16_t(v >> 16));
    };
    word(0x4d42);
    integer(54 + 64 * 32 * 4);
    integer(0);
    integer(54);
    integer(40);
    integer(64);
    integer(32);
    word(1);
    word(32);
    integer(0);
    integer(64 * 32 * 4);
    integer(0);
    integer(0);
    integer(0);
    integer(0);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 64; ++x) {
            // Four colored areas with a cross for orientation and tracing QA.
            bool road = std::abs(x - 32) <= 2 || std::abs(y - 16) <= 2;
            bitmap.push_back(road ? 210 : uint8_t(x < 32 ? 70 : 180));
            bitmap.push_back(road ? 210 : uint8_t(y < 16 ? 130 : 70));
            bitmap.push_back(road ? 210 : uint8_t(x < 32 ? 90 : 150));
            bitmap.push_back(255);
        }
    writeFileAtomic(fixture, bitmap);
    const auto view = a.view;
    const auto serialized = a.world.serialize();
    const bool dirty = a.world.dirty();
    auto render = [&] {
        auto draw = std::make_unique<ImDrawList>(ImGui::GetDrawListSharedData());
        draw->_ResetForNewFrame();
        draw->PushTextureID(ImGui::GetIO().Fonts->TexID);
        draw->PushClipRect({0, 0}, ImGui::GetIO().DisplaySize);
        drawMap(a, draw.get());
        draw->PopClipRect();
        draw->PopTextureID();
        return draw;
    };
    auto sameMap = [&](const ImDrawList &left, const ImDrawList &right) {
        r
equire(left.VtxBuffer.Size == right.VtxBuffer.Size &&
                    left.IdxBuffer.Size == right.IdxBuffer.Size &&
                    left.CmdBuffer.Size == right.CmdBuffer.Size,
                "Hidden reference changed map drawing");
        require(std::memcmp(left.VtxBuffer.Data, right.VtxBuffer.Data,
                            left.VtxBuffer.Size * sizeof(ImDrawVert)) == 0 &&
                    std::memcmp(left.IdxBuffer.Data, right.IdxBuffer.Data,
                                left.IdxBuffer.Size * sizeof(ImDrawIdx)) == 0,
                "Hidden reference changed map vertices or indices");
        for (int i = 0; i < left.CmdBuffer.Size; ++i) {
            const auto &l = left.CmdBuffer[i], &r = right.CmdBuffer[i];
            require(l.GetTexID() == r.GetTexID() && l.ElemCount == r.ElemCount &&
                        l.IdxOffset == r.IdxOffset && l.VtxOffset == r.VtxOffset &&
                        std::memcmp(&l.ClipRect, &r.ClipRect, sizeof(ImVec4)) == 0,
                    "Hidden reference changed map draw commands");
        }
    };
    auto baseline = render();
    a.loadReferenceImage(fixture);
    auto &r = a.referenceImage;
    require(r.texture && r.width == 64 && r.height == 32 && r.center == a.view.center,
            "Reference did not decode and center in view");
    const auto texture = r.texture;
    bool rejected = false;
    try {
        a.loadReferenceImage(fixture.parent_path() / L"missing-reference.png");
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected && r.texture == texture && r.path == fixture,
            "Failed replacement discarded the working reference");
    a.clearImages();
    require(r.texture == texture, "Refreshing resource images discarded the reference");
    r.opacity = 0;
    sameMap(*baseline, *render());
    r.opacity = .5f;
    r.scale = 2;
    r.rotation = 0;
    auto visible = render();
    require(visible->CmdBuffer.Size >= 3 && visible->CmdBuffer[0].ElemC
ount == 6 &&
                visible->CmdBuffer[1].GetTexID() == texture &&
                visible->CmdBuffer[1].ElemCount == 6 &&
                visible->CmdBuffer[2].GetTexID() != texture && visible->CmdBuffer[2].ElemCount > 0,
            "Reference must draw after background and before grid and level geometry");
    const auto &clip = visible->CmdBuffer[1].ClipRect;
    require(clip.x == a.view.origin.x && clip.y == a.view.origin.y &&
                clip.z == a.view.origin.x + a.view.size.x &&
                clip.w == a.view.origin.y + a.view.size.y,
            "Reference escaped the map canvas clip");
    require((visible->VtxBuffer[4].col >> IM_COL32_A_SHIFT) == 128,
            "Reference opacity did not reach the renderer");
    auto close = [](float l, float r) { return std::abs(l - r) < .02f; };
    auto topLeft = screenToWorld(a, visible->VtxBuffer[4].pos);
    require(close(topLeft.x, r.center.x - 64) && close(topLeft.y, r.center.y + 32) &&
                visible->VtxBuffer[4].uv.x == 0 && visible->VtxBuffer[4].uv.y == 0,
            "Reference dimensions or orientation are incorrect");
    a.view.center.x += 123;
    a.view.center.y -= 45;
    a.view.zoom *= 2;
    auto moved = render();
    topLeft = screenToWorld(a, moved->VtxBuffer[4].pos);
    require(close(topLeft.x, r.center.x - 64) && close(topLeft.y, r.center.y + 32),
            "Pan or zoom detached the reference from map coordinates");
    r.rotation = 90;
    auto rotated = render();
    topLeft = screenToWorld(a, rotated->VtxBuffer[4].pos);
    require(close(topLeft.x, r.center.x - 32) && close(topLeft.y, r.center.y - 64),
            "Reference rotation changed its center or aspect ratio");
    a.view = view;
    const auto retired = a.retiredImages.size();
    a.loadReferenceImage(fixture);
    require(a.retiredImages.size() == retired + 1 && a.retiredImages.back() == texture,
            "Reference replacement did not defer texture release");
    const auto replacement = r.texture;

    a.removeReferenceImage();
    require(!r.texture && r.path.empty() && a.retiredImages.back() == replacement,
            "Reference removal did not clear state and retire the texture");
    sameMap(*baseline, *render());
    require(a.world.serialize() == serialized && a.world.dirty() == dirty,
            "Reference controls modified the level");
    {
        auto saved = a.world;
        ReferenceSettings expected{std::filesystem::absolute(fixture).lexically_normal().u8string(),
                                   {123.25f, -47.5f},
                                   2.125f,
                                   .375f,
                                   27.5f};
        auto document = fixture.parent_path() / "REFERENCE.dwd";
        a.world.reference = expected;
        a.world.save(document);
        a.world.load(document);
        a.syncReferenceImage();
        require(a.referenceImage.texture && a.world.reference == expected &&
                    a.referenceImage.center == expected.center &&
                    a.referenceImage.scale == expected.scale &&
                    a.referenceImage.rotation == expected.rotation &&
                    a.referenceImage.opacity == expected.opacity,
                "Saved reference image or transforms did not reopen");
        a.world.checkpoint();
        a.world.reference.scale = 3.5f;
        a.syncReferenceImage();
        require(a.world.undo(), "Reference transform undo failed");
        a.syncReferenceImage();
        require(a.referenceImage.scale == expected.scale,
                "Undo did not restore visible reference transform");
        a.world.reference.path = (fixture.parent_path() / "missing-reference.png").u8string();
        a.syncReferenceImage();
        require(!a.referenceImage.texture && a.world.reference.scale == expected.scale &&
                    a.referenceImage.center == expected.center,
                "Missing image discarded saved reference settings");
        a.world = std::move(saved)
;
        a.removeReferenceImage(false);
    }
    // Keep a visible reference for the smoke screenshot.
    a.loadReferenceImage(fixture);
    ImGui::Begin("##native");
    ImGui::OpenPopup("Background reference");
    ImGui::End();
}
} // namespace de
