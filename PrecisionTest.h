#ifndef BASEX_DEMO_PRECISION_TEST_H
#define BASEX_DEMO_PRECISION_TEST_H

#include "BaseX/Engine.h"
#include <vector>

namespace BaseX {
namespace Demos {

// Precision test demo - validates DRO accuracy across distance ranges
class PrecisionTestDemo : public DemoBase {
public:
    PrecisionTestDemo() = default;
    ~PrecisionTestDemo() override = default;

    bool initialize(Engine* engine) override;
    void shutdown() override;
    void update(float deltaTime) override;
    void render() override;

    const char* getName() const override { return "Precision Test"; }
    const char* getDescription() const override { 
        return "Tests DRO precision across VR distance ranges"; 
    }
    const char* getCategory() const override { return "Testing"; }

private:
    struct TestObject {
        float distance;           // Distance from origin
        RadixBase expectedRadix;  // Expected optimal radix
        float measuredError;      // Measured precision error
        bool passed;              // Test passed?
        LODManager::LODLevel lodLevel;
    };

    std::vector<TestObject> m_testObjects;
    float m_testTimer = 0.0f;
    int m_currentTest = 0;
    bool m_testsComplete = false;

    void setupTests();
    void runTest(int index);
    void logResults();
};

} // namespace Demos
} // namespace BaseX

#endif // BASEX_DEMO_PRECISION_TEST_H
