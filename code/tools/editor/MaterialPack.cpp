#include "Wadfile.h"
#include "idfunctions.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <unordered_set>

namespace de {
using Microsoft::WRL::ComPtr;

std::vector<doom_materials::Definition> Wadfile::materials() const {
    std::vector<doom_materials::Definition> result;
    std::unordered_set<std::string> names;
    for (const auto &lump : lumps) {
        if (lump.name != "DMATDEF")
            continue;
        for (auto &d : doom_materials::parse(std::string(lump.data.begin(), lump.data.end()))) {
            require(names.insert(d.name).second, "Duplicate material: " + d.name);
            result.push_back(std::move(d));
        }
    }
    return result;
}

Image Wadfile::materialImage(const std::string &name) const {
    const auto loose = mapImages.find(upper(name));
    const auto bytes = loose == mapImages.end() ? get(name) : readFile(loose->second);
    require(bytes.size() >= 8 && bytes.size() <= 128 * 1024 * 1024 &&
                memcmp(bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0,
            "Material is not a PNG: " + name);
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct Apartment {
        bool owned;
        ~Apartment() {
            if (owned)
                CoUninitialize();
        }
    } apartment{SUCCEEDED(initialized)};
    auto check = [&](HRESULT result) {
        require(SUCCEEDED(result), "Cannot decode material PNG: " + name);
    };
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;
    check(factory->CreateStream(&stream));
    check(stream->InitializeFromMemory(const_cast<BYTE *>(bytes.data()), DWORD(bytes.size())));
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
                                           &decoder));
    ComPtr<IWICBitmapFrameDecode> frame;
    check(decoder->GetFrame(0, &frame));
    UINT width, height;
    check(frame->GetSize(&width, &height));
    require(width && height && width <= 4096 && height <= 4096,
            "Material PNG dimensions must be 1..4096");
    ComPtr<IWICFormatConverter> converter;
    check(factory->CreateFormatConverter(&converter));
    check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                nullptr, 0, WICBitmapPaletteTypeCustom));
    Image image{int(width), int(height), std::vector<uint32_t>(size_t(width) * height)};
    check(converter->CopyPixels(nullptr, width * 4, width * height * 4,
                                reinterpret_cast<BYTE *>(image.pixels.data())));
    return image;
}

void Wadfile::mergeMaterials(const Wadfile &pack) {
    auto incoming = pack.materials();
    require(!incoming.empty(), "No DMATDEF materials found in this WAD");
    const auto walls = textures();
    require(materials().size() + incoming.size() <= 4096, "At most 4096 PNG materials per project");
    std::unordered_set<std::string> existing;
    for (const auto &d : materials())
        existing.insert(d.name);
    for (const auto &t : walls)
        existing.insert(t.name);
    // Validate everything before mutating the resource WAD.
    for (const auto &d : incoming) {
        require(!existing.count(d.name) && find(d.name) < 0,
                "Material name already exists (replacement is not allowed): " + d.name);
        require(std::count_if(pack.lumps.begin(), pack.lumps.end(),
                              [&](const Lump &l) { return l.name == d.name; }) == 1,
                "Material must have exactly one PNG lump: " + d.name);
        pack.materialImage(d.name);
    }
    auto definitions = materials();
    definitions.insert(definitions.end(), incoming.begin(), incoming.end());
    const auto manifest = doom_materials::serialize(definitions);
    lumps.erase(std::remove_if(lumps.begin(), lumps.end(),
                               [](const Lump &l) { return l.name == "DMATDEF"; }),
                lumps.end());
    add("DMATDEF", Bytes(manifest.begin(), manifest.end()));
    for (const auto &d : incoming)
        add(d.name, pack.get(d.name));
}

void Wadfile::copyMaterialsTo(Wadfile &target) const {
    const auto definitions = materials();
    if (definitions.empty())
        return;
    require(target.find("DMATDEF") < 0, "Destination already contains a material manifest");
    const auto manifest = doom_materials::serialize(definitions);
    target.add("DMATDEF", Bytes(manifest.begin(), manifest.end()));
    for (const auto &d : definitions) {
        require(target.find(d.name) < 0, "Material collides with map lump: " + d.name);
        target.add(d.name, get(d.name));
    }
}
} // namespace de
