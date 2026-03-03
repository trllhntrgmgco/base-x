// Minimal utils

#include <iostream>
#include <string>

namespace basex {

void log(const std::string& message) {
    std::cout << "[LOG] " << message << std::endl;
}

float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

} // namespace basex
