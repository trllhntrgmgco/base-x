#include <iostream>
#include <BaseX/DRO.h>

int main() {
    BaseX::DROManager dro;
    dro.initializeVRProfile();
    float distances[] = {0.1f, 0.7f, 3.0f, 25.0f, 500.0f};
    for (float d : distances) {
        auto lvl = dro.getPrecisionLevel(d);
        if (lvl) {
            std::cout << "Distance: " << d << "m -> radix: " << static_cast<int>(lvl->radix) << " maxError: " << lvl->maxError << "\n";
        }
    }
    return 0;
}
