#include "LineSpecialRemapper.h"
namespace de {
int remapSpecials(EditWorld &w, int from, int to) {
    int n = 0;
    for (auto &l : w.lines)
        if (l.selected >= 0 && l.special == from) {
            l.special = to;
            ++n;
        }
    return n;
}
} // namespace de
