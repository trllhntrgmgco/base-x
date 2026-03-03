#include <iostream>
#include <vector>
#include <cmath>
#include "BaseX/Engine.h"
#include "BaseX/DRO.h"

struct Hole {
    float distance; // Distance to hole
    std::string name;
};

class GolfDemo {
public:
    GolfDemo() : currentHole(0), ballPosition(0.0f) {
        holes = {
            {100.0f, "Hole 1"},
            {150.0f, "Hole 2"},
            {200.0f, "Hole 3"},
            {120.0f, "Hole 4"},
            {180.0f, "Hole 5"},
            {90.0f, "Hole 6"},
            {160.0f, "Hole 7"},
            {140.0f, "Hole 8"},
            {110.0f, "Hole 9"}
        };
    }

    void play(basex::Engine& engine) {
        BaseX::DROManager dro;
        dro.initializeVRProfile();

        for (int i = 0; i < 9; ++i) {
            currentHole = i;
            ballPosition = 0.0f;
            std::cout << "Playing " << holes[i].name << " - Distance: " << holes[i].distance << "m\n";

            while (ballPosition < holes[i].distance) {
                // Simulate swing
                float power = 50.0f; // Fixed for demo
                float distanceToHole = holes[i].distance - ballPosition;

                // Use DRO for precision
                BaseX::RadixBase radix = dro.getOptimalRadix(distanceToHole);
                float accuracy = (radix == BaseX::RadixBase::BASE_16) ? 0.1f : 0.01f; // Higher precision closer

                float hitDistance = power * (1.0f + (rand() % 10 - 5) * accuracy / 10.0f);
                ballPosition += hitDistance;

                std::cout << "Hit ball " << hitDistance << "m. Total: " << ballPosition << "m. Radix: " << static_cast<int>(radix) << "\n";

                if (ballPosition >= holes[i].distance) {
                    std::cout << "Hole in " << (i + 1) << "!\n";
                    break;
                }

                // Simulate LOD: Detail level based on distance
                int lod = (distanceToHole > 50.0f) ? 1 : 2; // Low detail far, high close
                std::cout << "LOD Level: " << lod << "\n";

                engine.update(0.016f);
            }
        }
        std::cout << "Golf demo complete!\n";
    }

private:
    std::vector<Hole> holes;
    int currentHole;
    float ballPosition;
};

int main() {
    basex::Engine engine;
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize engine.\n";
        return 1;
    }

    GolfDemo golf;
    golf.play(engine);

    engine.shutdown();
    return 0;
}