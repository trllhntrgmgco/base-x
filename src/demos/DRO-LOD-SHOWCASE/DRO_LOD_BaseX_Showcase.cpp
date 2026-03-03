#include <iostream>
#include <BaseX/DRO.h>

int main() {
    BaseX::DROManager dro;
    dro.initializeVRProfile();
    BaseX::LODManager lod(&dro);

    float testDistances[] = {0.2f, 1.0f, 5.0f, 20.0f, 200.0f};
    for (float d : testDistances) {
        auto lvl = dro.getPrecisionLevel(d);
        auto settings = lod.getLODSettings(d);
        std::cout << "Distance: " << d << " -> LOD: " << static_cast<int>(settings.level) << " radix: " << static_cast<int>(settings.radix) << " polyReduction: " << settings.polyReduction << "\n";
    }
    return 0;
}
