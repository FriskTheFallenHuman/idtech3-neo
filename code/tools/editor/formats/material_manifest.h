#pragma once
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
#include <cmath>

// Shared by the game, editor and pack tests. Names live in the map's existing
// eight-character texture fields; PNG resolution is independent of map scale.
namespace doom_materials {
struct Emission {
    float red = 1, green = 1, blue = 1, strength = 0, range = 768;
    bool flicker = false;
    unsigned strobePeriod = 0, strobePhase = 0;
    bool blink = false;
    bool point = false;
    float pointX = 0, pointY = 0, pointZ = 0;
    float coronaRadius = 0, coronaStrength = 0;
};
// Deterministic 35 Hz electrical dropout; does not consume gameplay RNG.
inline float emissionBrightness(const Emission &e, unsigned tick) {
    if (e.strobePeriod) {
        const unsigned phase = (tick % e.strobePeriod + e.strobePhase) % e.strobePeriod;
        if (e.blink)
            return phase < (e.strobePeriod + 1) / 2 ? 1.f : 0.f;
        // Two short flashes in the first half-cycle, followed by a dark half.
        const unsigned pulse = std::max(1u, e.strobePeriod / 10);
        return phase < pulse || (phase >= 2 * pulse && phase < 3 * pulse) ? 1.f : 0.f;
    }
    if (!e.flicker)
        return 1.f;
    unsigned burst = tick / 35;
    unsigned hash = burst * 747796405u + 2891336453u;
    hash = ((hash >> ((hash >> 28u) + 4u)) ^ hash) * 277803737u;
    unsigned phase = tick % 35;
    unsigned start = (hash >> 16) % 18;
    if (phase >= start && phase < start + 3)
        return .025f;
    if (phase == start + 5 || phase == start + 8)
        return .12f;
    if ((hash & 3) == 0 && phase >= start + 10 && phase < start + 15)
        return .04f;
    return 1.f;
}
inline Emission parseEmission(std::istream &fields) {
    Emission e;
    if (!(fields >> e.red >> e.green >> e.blue >> e.strength >> e.range) || !std::isfinite(e.red) ||
        !std::isfinite(e.green) || !std::isfinite(e.blue) || !std::isfinite(e.strength) ||
        !std::isfinite(e.range) || e.red < 0 || e.red > 1 || e.green < 0 || e.green > 1 ||
        e.blue < 0 || e.blue > 1 || e.strength <= 0 || e.strength > 10000 || e.range <= 0 ||
        e.range > 16384)
        throw std::runtime_error(
            "Invalid EMISSIVE: expected RGB 0..1, strength 0..10000, range 0..16384");
    std::string option;
    while (fields >> option) {
        if (option == "FLICKER" && !e.flicker && !e.strobePeriod)
            e.flicker = true;
        else if ((option == "STROBE" || option == "BLINK") && !e.flicker && !e.strobePeriod) {
            int period, phase;
            if (!(fields >> period >> phase) || period < 10 || period > 350 || phase < 0 ||
                phase >= period)
                throw std::runtime_error(
                    "STROBE expects period 10..350 and phase 0..period-1 (35 Hz ticks)");
            e.strobePeriod = unsigned(period);
            e.strobePhase = unsigned(phase);
            e.blink = option == "BLINK";
        } else if (option == "POINT" && !e.point) {
            if (!(fields >> e.pointX >> e.pointY >> e.pointZ) || !std::isfinite(e.pointX) ||
                !std::isfinite(e.pointY) || !std::isfinite(e.pointZ) || std::abs(e.pointX) > 4096 ||
                std::abs(e.pointY) > 4096 || std::abs(e.pointZ) > 4096)
                throw std::runtime_error("POINT expects a finite model-local XYZ attachment");
            e.point = true;
        } else if (option == "CORONA" && e.coronaRadius == 0) {
            if (!(fields >> e.coronaRadius >> e.coronaStrength) || !std::isfinite(e.coronaRadius) ||
                !std::isfinite(e.coronaStrength) || e.coronaRadius <= 0 || e.coronaRadius > 256 ||
                e.coronaStrength <= 0 || e.coronaStrength > 4)
                throw std::runtime_error("CORONA expects radius 0..256 and strength 0..4");
        } else
            throw std::runtime_error("Unknown or duplicate EMISSIVE option: " + option);
    }
    if (!fields.eof())
        throw std::runtime_error("Invalid EMISSIVE options");
    return e;
}
struct Definition {
    char kind = 'W'; // W: wall, F: floor/ceiling, S: panoramic sky in the flat palette
    std::string name;
    int width = 128, height = 128;
    Emission emission;
};
inline std::vector<Definition> parse(const std::string &text) {
    if (text.size() > 1024 * 1024)
        throw std::runtime_error("Material manifest is too large");
    std::istringstream input(text);
    std::string row;
    if (!std::getline(input, row) || (row != "DOOM_MATERIALS 1" && row != "DOOM_MATERIALS 1\r"))
        throw std::runtime_error("Expected DOOM_MATERIALS 1 manifest header");
    std::vector<Definition> result;
    std::unordered_set<std::string> names;
    while (std::getline(input, row)) {
        row.erase(row.find('#') == std::string::npos ? row.size() : row.find('#'));
        std::istringstream fields(row);
        Definition d;
        if (!(fields >> d.kind))
            continue;
        std::string extra;
        if (!(fields >> d.name >> d.width >> d.height) ||
            (d.kind != 'W' && d.kind != 'F' && d.kind != 'S') || d.name.empty() ||
            d.name.size() > 8 ||
            d.name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") !=
                std::string::npos ||
            d.width < 1 || d.width > 4096 || d.height < 1 || d.height > 4096 ||
            d.name == "DMATDEF" || !names.insert(d.name).second || result.size() >= 4096)
            throw std::runtime_error("Invalid or duplicate material definition: " + row);
        if (fields >> extra) {
            if (extra != "EMISSIVE" || d.kind == 'S')
                throw std::runtime_error("Invalid material option: " + row);
            d.emission = parseEmission(fields);
            if (d.emission.point)
                throw std::runtime_error("POINT attachments belong in model PNG.material sidecars");
            if (fields >> extra)
                throw std::runtime_error("Trailing material options: " + row);
        }
        result.push_back(d);
    }
    return result;
}
inline std::string serialize(const std::vector<Definition> &definitions) {
    std::ostringstream out;
    out << "DOOM_MATERIALS 1\n";
    for (const auto &d : definitions) {
        out << d.kind << ' ' << d.name << ' ' << d.width << ' ' << d.height;
        const auto &e = d.emission;
        if (e.strength > 0)
            out << " EMISSIVE " << e.red << ' ' << e.green << ' ' << e.blue << ' ' << e.strength
                << ' ' << e.range;
        if (e.strength > 0 && e.flicker)
            out << " FLICKER";
        if (e.strength > 0 && e.strobePeriod)
            out << (e.blink ? " BLINK " : " STROBE ") << e.strobePeriod << ' ' << e.strobePhase;
        if (e.strength > 0 && e.point)
            out << " POINT " << e.pointX << ' ' << e.pointY << ' ' << e.pointZ;
        if (e.strength > 0 && e.coronaRadius > 0)
            out << " CORONA " << e.coronaRadius << ' ' << e.coronaStrength;
        out << '\n';
    }
    return out.str();
}
} // namespace doom_materials
