#pragma once
#include "R_mapdef.h"
#include <filesystem>
namespace de {
class Wadfile;
struct WorldState {
    std::vector<Point> points;
    std::vector<Line> lines;
    std::vector<Thing> things;
    std::vector<ThingSpline> thingSplines;
    ReferenceSettings reference;
    bool explicitSectors = false;
};
class EditWorld : public WorldState {
    std::vector<WorldState> undoStates, redoStates;
    WorldState clipboard;
    std::string saved;

  public:
    std::filesystem::path pathname;
    SectorDef adjustedSurface(const SectorDef &s, bool ceiling, int delta, int slopeDelta,
                              const std::vector<std::pair<int, int>> &boundaries = {}) const;
    SectorDef adjustedFloor(const SectorDef &s, int delta, int slopeDelta,
                            const std::vector<std::pair<int, int>> &boundaries = {}) const;
    void checkpoint();
    bool undo();
    bool redo();
    bool dirty() const;
    void markSaved();
    void clear();
    void deselect();
    void selectAll(int mode);
    int newPoint(Vec2 p);
    void placeObject(Vec2 point, int type, int angle = 0, int options = 7);
    int newLine(Vec2 a, Vec2 b, const Line &prototype = Line{});
    void deleteSelected();
    void flipSelected();
    void fusePoints();
    void separatePoints();
    void splitLine(int index, Vec2 at);
    void moveSelected(Vec2 delta);
    bool moveSelectedThingsVertically(int delta);
    int nextSplineId() const;
    ThingSpline *findSpline(int id);
    const ThingSpline *findSpline(int id) const;
    void rebuildSpline(int id);
    void deleteSpline(int id);
    void bakeSpline(int id);
    void copy();
    void paste(Vec2 at);
    Vec2 selectionCenter() const;
    void load(const std::filesystem::path &path);
    void save(const std::filesystem::path &path);
    std::string serialize() const;
    std::string serializeForCompiler() const;
    void parse(const std::string &text);
    void importMap(const Wadfile &wad, const std::string &marker);
    void exportMap(const std::filesystem::path &path, const std::string &marker) const;
    void exportLegacy(const std::filesystem::path &path, const std::string &marker,
                      const std::vector<Texture> &textures) const;
    std::vector<std::string> validate(bool checkTextures = true) const;
};
} // namespace de
