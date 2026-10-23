#include "lbmfunctions.h"
#include "idfunctions.h"
#include <algorithm>
namespace de {
static uint32_t be32(const Bytes &b, size_t p) {
    require(p + 4 <= b.size(), "Truncated IFF");
    return uint32_t(b[p]) << 24 | uint32_t(b[p + 1]) << 16 | uint32_t(b[p + 2]) << 8 | b[p + 3];
}
Image loadLBM(const std::filesystem::path &p) {
    auto b = readFile(p);
    require(b.size() >= 12 && std::string(b.begin(), b.begin() + 4) == "FORM", "Not an IFF image");
    bool planar = std::string(b.begin() + 8, b.begin() + 12) == "ILBM";
    require(planar || std::string(b.begin() + 8, b.begin() + 12) == "PBM ",
            "Unsupported IFF image");
    int w = 0, h = 0, planes = 8, mask = 0, compression = 0;
    Bytes pal, body;
    size_t end = size_t(be32(b, 4)) + 8;
    require(end <= b.size(), "Truncated FORM");
    for (size_t q = 12; q + 8 <= end;) {
        std::string id(b.begin() + q, b.begin() + q + 4);
        size_t n = be32(b, q + 4);
        q += 8;
        require(n <= end - q, "Truncated IFF chunk");
        if (id == "BMHD") {
            require(n >= 20, "Truncated BMHD");
            w = b[q] * 256 + b[q + 1];
            h = b[q + 2] * 256 + b[q + 3];
            planes = b[q + 8];
            mask = b[q + 9];
            compression = b[q + 10];
        } else if (id == "CMAP")
            pal.assign(b.begin() + q, b.begin() + q + n);
        else if (id == "BODY")
            body.assign(b.begin() + q, b.begin() + q + n);
        q += n + (n & 1);
    }
    require(w > 0 && h > 0 && w <= 4096 && h <= 4096 && planes >= 1 && planes <= 8 &&
                compression <= 1 && mask <= 2,
            "Unsupported LBM dimensions or encoding");
    int stride = planar ? ((w + 15) / 16) * 2 : ((w + 1) & ~1);
    int rowplanes = planar ? planes + (mask == 1) : 1;
    Bytes raw;
    size_t expected = size_t(stride) * h * rowplanes;
    raw.reserve(expected);
    size_t pos = 0;
    while (raw.size() < expected) {
        require(pos < body.size(), "Truncated LBM body");
        if (!compression) {
            raw = body;
            break;
        }
        int8_t cmd = int8_t(body[pos++]);
        if (cmd >= 0) {
            size_t n = size_t(cmd) + 1;
            require(n <= body.size() - pos && n <= expected - raw.size(), "Invalid LBM run");
            raw.insert(raw.end(), body.begin() + pos, body.begin() + pos + n);
            pos += n;
        } else if (cmd != -128) {
            size_t n = 1 - int(cmd);
            require(pos < body.size() && n <= expected - raw.size(), "Invalid LBM repeat");
            raw.insert(raw.end(), n, body[pos++]);
        }
    }
    require(raw.size() >= expected, "Truncated LBM pixels");
    Image im{w, h, std::vector<uint32_t>(size_t(w) * h)};
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int v = 0;
            if (planar) {
                for (int bit = 0; bit < planes; ++bit)
                    v |= ((raw[(size_t(y) * rowplanes + bit) * stride + x / 8] >> (7 - x % 8)) & 1)
                         << bit;
            } else
                v = raw[size_t(y) * stride + x];
            uint32_t c =
                size_t(v) * 3 + 2 < pal.size()
                    ? pal[v * 3] | uint32_t(pal[v * 3 + 1]) << 8 | uint32_t(pal[v * 3 + 2]) << 16
                    : uint32_t(v) * 0x010101;
            im.pixels[size_t(y) * w + x] = 0xff000000u | c;
        }
    return im;
}
void saveLBM(const std::filesystem::path &p, const Bytes &indices, int w, int h, const Bytes &pal) {
    require(w > 0 && h > 0 && w <= 4096 && h <= 4096 && indices.size() == size_t(w) * h &&
                pal.size() == 768,
            "Invalid LBM image");
    auto big = [](Bytes &b, uint32_t v) {
        for (int i = 3; i >= 0; --i)
            b.push_back(uint8_t(v >> (i * 8)));
    };
    Bytes b = {'F', 'O', 'R', 'M', 0, 0, 0, 0, 'P', 'B', 'M', ' '};
    auto chunk = [&](const char *id, const Bytes &data) {
        b.insert(b.end(), id, id + 4);
        big(b, uint32_t(data.size()));
        b.insert(b.end(), data.begin(), data.end());
        if (data.size() & 1)
            b.push_back(0);
    };
    Bytes header(20);
    header[0] = uint8_t(w >> 8);
    header[1] = uint8_t(w);
    header[2] = uint8_t(h >> 8);
    header[3] = uint8_t(h);
    header[8] = 8;
    header[14] = header[15] = 1;
    header[16] = header[0];
    header[17] = header[1];
    header[18] = header[2];
    header[19] = header[3];
    chunk("BMHD", header);
    chunk("CMAP", pal);
    Bytes body;
    for (int y = 0; y < h; ++y) {
        body.insert(body.end(), indices.begin() + size_t(y) * w,
                    indices.begin() + size_t(y + 1) * w);
        if (w & 1)
            body.push_back(0);
    }
    chunk("BODY", body);
    uint32_t n = uint32_t(b.size() - 8);
    for (int i = 0; i < 4; ++i)
        b[4 + i] = uint8_t(n >> ((3 - i) * 8));
    writeFileAtomic(p, b);
}
} // namespace de
