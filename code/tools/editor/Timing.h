#pragma once
#include <chrono>
namespace de {
class Timing {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

  public:
    double seconds() const;
    void reset();
};
} // namespace de
