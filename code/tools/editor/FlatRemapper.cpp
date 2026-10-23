#include "FlatRemapper.h"
namespace de {
int remapFlats(EditWorld &w, const std::string &from, const std::string &to) {
    int n = 0;
    for (auto &l : w.lines)
        if (l.selected >= 0)
            for (int i = 0; i < ((l.flags & 4) ? 2 : 1); ++i) {
                auto &e = l.side[i].ends;
                for (auto *name : {&e.floorflat, &e.ceilingflat})
                    if (*name == from) {
                        *name = to;
                        ++n;
                    }
            }
    return n;
}
} // namespace de
