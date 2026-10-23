#include "Timing.h"
namespace de {
double Timing::seconds() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}
void Timing::reset() {
    start = std::chrono::steady_clock::now();
}
} // namespace de
