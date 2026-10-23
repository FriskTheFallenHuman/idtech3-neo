#include "ThingRemapper.h"
namespace de {
int remapThings(EditWorld &w, int from, int to) {
    int n = 0;
    for (auto &t : w.things)
        if (t.selected >= 0 && t.type == from) {
            t.type = to;
            ++n;
        }
    return n;
}
} // namespace de
