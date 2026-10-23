#include <windows.h>
#include "NextUI.h"
#include "idfunctions.h"
#include <wincodec.h>
#include <wrl/client.h>
namespace de {
Image nextBitmap(const std::string &name) {
    using Microsoft::WRL::ComPtr;
    auto module = GetModuleHandleW(nullptr);
    auto resourceName = upper(name);
    std::wstring wide(resourceName.begin(), resourceName.end());
    auto resource = FindResourceW(module, wide.c_str(), MAKEINTRESOURCEW(10));
    require(resource != nullptr, "Missing embedded original artwork: " + name);
    auto data = LoadResource(module, resource);
    auto bytes = static_cast<BYTE *>(LockResource(data));
    auto length = SizeofResource(module, resource);
    ComPtr<IWICImagingFactory> factory;
    require(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&factory))),
            "Cannot initialize image decoder");
    ComPtr<IWICStream> stream;
    factory->CreateStream(&stream);
    require(SUCCEEDED(stream->InitializeFromMemory(bytes, length)), "Cannot read original image");
    ComPtr<IWICBitmapDecoder> decoder;
    require(SUCCEEDED(factory->CreateDecoderFromStream(stream.Get(), nullptr,
                                                       WICDecodeMetadataCacheOnLoad, &decoder)),
            "Cannot decode original TIFF: " + name);
    ComPtr<IWICBitmapFrameDecode> frame;
    decoder->GetFrame(0, &frame);
    ComPtr<IWICFormatConverter> converter;
    factory->CreateFormatConverter(&converter);
    require(SUCCEEDED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                            WICBitmapDitherTypeNone, nullptr, 0,
                                            WICBitmapPaletteTypeCustom)),
            "Cannot convert original TIFF");
    UINT w, h;
    converter->GetSize(&w, &h);
    Image image;
    image.width = w;
    image.height = h;
    image.pixels.resize(size_t(w) * h);
    require(SUCCEEDED(converter->CopyPixels(nullptr, w * 4, w * h * 4,
                                            reinterpret_cast<BYTE *>(image.pixels.data()))),
            "Cannot read original TIFF pixels");
    return image;
}
} // namespace de
