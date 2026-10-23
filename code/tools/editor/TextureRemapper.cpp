#include "TextureRemapper.h"
namespace de {
int remapTextures(EditWorld &w, const std::string &from, const std::string &to) {
    int n = 0;
    for (auto &l : w.lines)
        if (l.selected >= 0)
            for (int i = 0; i < ((l.flags & 4) ? 2 : 1); ++i) {
                auto &s = l.side[i];
                for (auto *name : {&s.toptexture, &s.midtexture, &s.bottomtexture})
                    if (*name == from) {
                        *name = to;
                        ++n;
                    }
            }
    return n;
}
} // namespace de
