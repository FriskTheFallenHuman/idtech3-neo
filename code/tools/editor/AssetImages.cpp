// SPDX-License-Identifier: GPL-3.0-or-later
#include "AssetImages.h"
#include "UI.h"
#include "idfunctions.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <fstream>
namespace de::sprite_baker {
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT hr, const char *message) {
    require(SUCCEEDED(hr), message);
}
std::vector<char> readBytes(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    require(bool(in), "Cannot open " + path.u8string());
    auto size = in.tellg();
    require(size > 0 && size <= 512 * 1024 * 1024, "Asset must be between 1 byte and 512 MB");
    std::vector<char> bytes(static_cast<size_t>(size));
    in.seekg(0);
    in.read(bytes.data(), size);
    require(bool(in), "Cannot read " + path.u8string());
    return bytes;
}
} // namespace
Image decodeImage(const void *bytes, size_t size) {
    require(size && size < 128 * 1024 * 1024, "Invalid image size");
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&factory)),
          "Cannot create WIC");
    ComPtr<IWICStream> stream;
    check(factory->CreateStream(&stream), "Cannot create image stream");
    check(stream->InitializeFromMemory(const_cast<BYTE *>(static_cast<const BYTE *>(bytes)),
                                       static_cast<DWORD>(size)),
          "Cannot read image bytes");
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
                                           &decoder),
          "Unsupported image encoding");
    ComPtr<IWICBitmapFrameDecode> frame;
    check(decoder->GetFrame(0, &frame), "Cannot decode image frame");
    UINT w, h;
    check(frame->GetSize(&w, &h), "Cannot read image dimensions");
    require(w && h && w <= 8192 && h <= 8192 && uint64_t(w) * h <= 32 * 1024 * 1024,
            "Image is too large");
    ComPtr<IWICFormatConverter> converter;
    check(factory->CreateFormatConverter(&converter), "Cannot create image converter");
    check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                nullptr, 0, WICBitmapPaletteTypeCustom),
          "Cannot convert image to RGBA");
    Image image{static_cast<int>(w), static_cast<int>(h), std::vector<uint32_t>(size_t(w) * h)};
    check(converter->CopyPixels(nullptr, w * 4, UINT(image.pixels.size() * 4),
                                (BYTE *)image.pixels.data()),
          "Cannot copy image pixels");
    return image;
}
Image readImage(const std::filesystem::path &path) {
    const auto bytes = readBytes(path);
    return decodeImage(bytes.data(), bytes.size());
}
void writePNG(const std::filesystem::path &path, const Image &image) {
    require(image.width > 0 && image.height > 0 &&
                image.pixels.size() == size_t(image.width) * image.height,
            "Invalid PNG pixels");
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&factory)),
          "Cannot create PNG encoder");
    ComPtr<IWICStream> stream;
    check(factory->CreateStream(&stream), "Cannot create PNG stream");
    check(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "Cannot create PNG file");
    ComPtr<IWICBitmapEncoder> encoder;
    check(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder),
          "Cannot create PNG encoder");
    check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache),
          "Cannot initialize PNG encoder");
    ComPtr<IWICBitmapFrameEncode> frame;
    check(encoder->CreateNewFrame(&frame, nullptr), "Cannot create PNG frame");
    check(frame->Initialize(nullptr), "Cannot initialize PNG frame");
    check(frame->SetSize(image.width, image.height), "Cannot set PNG size");
    // Windows' PNG encoder accepts BGRA; our GPU buffers and public Image use RGBA.
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    check(frame->SetPixelFormat(&format), "Cannot set PNG pixel format");
    require(IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA), "PNG encoder changed pixel format");
    std::vector<uint32_t> bgra;
    bgra.reserve(image.pixels.size());
    for (uint32_t p : image.pixels)
        bgra.push_back((p & 0xff00ff00u) | ((p & 255u) << 16) | ((p >> 16) & 255u));
    check(frame->WritePixels(image.height, image.width * 4, static_cast<UINT>(bgra.size() * 4),
                             reinterpret_cast<BYTE *>(bgra.data())),
          "Cannot write PNG pixels");
    check(frame->Commit(), "Cannot finish PNG frame");
    check(encoder->Commit(), "Cannot finish PNG file");
}
std::filesystem::path packsDirectory() {
    return projectDirectory() / "base/sprites/packs";
}
} // namespace de::sprite_baker
