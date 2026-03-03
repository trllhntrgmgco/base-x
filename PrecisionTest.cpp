#include "BaseX/Demos/PrecisionTest.h"
#include "BaseX/Database.h"
#include <iostream>
#include <chrono>

namespace BaseX {
namespace Demos {

bool PrecisionTestDemo::initialize(Engine* engine) {
    m_engine = engine;
    setupTests();
    
    std::cout << "=== Base-X Precision Test Demo ===" << std::endl;
    std::cout << "Testing DRO across " << m_testObjects.size() << " distance ranges" << std::endl;
    
    return true;
}

void PrecisionTestDemo::shutdown() {
    if (m_testsComplete) {
        logResults();
    }
}

void PrecisionTestDemo::setupTests() {
    // Create test objects at various distances
    // These match the VR precision profiles
    
    // Handheld range (0-0.5m) - Base-64
    m_testObjects.push_back({0.1f, RadixBase::BASE_64, 0.0f, false, LODManager::LODLevel::LOD0_HIGHEST});
    m_testObjects.push_back({0.25f, RadixBase::BASE_64, 0.0f, false, LODManager::LODLevel::LOD0_HIGHEST});
    m_testObjects.push_back({0.45f, RadixBase::BASE_64, 0.0f, false, LODManager::LODLevel::LOD0_HIGHEST});
    
    // Near field (0.5-2m) - Base-32
    m_testObjects.push_back({0.75f, RadixBase::BASE_32, 0.0f, false, LODManager::LODLevel::LOD1_HIGH});
    m_testObjects.push_back({1.5f, RadixBase::BASE_32, 0.0f, false, LODManager::LODLevel::LOD1_HIGH});
    
    // Room scale (2-10m) - Base-16
    m_testObjects.push_back({3.0f, RadixBase::BASE_16, 0.0f, false, LODManager::LODLevel::LOD2_MEDIUM});
    m_testObjects.push_back({7.5f, RadixBase::BASE_16, 0.0f, false, LODManager::LODLevel::LOD2_MEDIUM});
    
    // Large space (10-100m) - Base-8
    m_testObjects.push_back({25.0f, RadixBase::BASE_8, 0.0f, false, LODManager::LODLevel::LOD3_LOW});
    m_testObjects.push_back({75.0f, RadixBase::BASE_8, 0.0f, false, LODManager::LODLevel::LOD3_LOW});
    
    // Distant (100m+) - Base-4
    m_testObjects.push_back({150.0f, RadixBase::BASE_4, 0.0f, false, LODManager::LODLevel::LOD4_LOWEST});
    m_testObjects.push_back({500.0f, RadixBase::BASE_4, 0.0f, false, LODManager::LODLevel::LOD4_LOWEST});
}

void PrecisionTestDemo::update(float deltaTime) {
    if (m_testsComplete) {
        return;
    }
    
    m_testTimer += deltaTime;
    
    // Run one test per second
    if (m_testTimer >= 1.0f && m_currentTest < m_testObjects.size()) {
        runTest(m_currentTest);
        m_currentTest++;
        m_testTimer = 0.0f;
        
        if (m_currentTest >= m_testObjects.size()) {
            m_testsComplete = true;
        }
    }
}

void PrecisionTestDemo::render() {
    // Rendering would display test objects at their distances
    // with color coding based on pass/fail status
}

void PrecisionTestDemo::runTest(int index) {
    auto& test = m_testObjects[index];
    
    DROManager* dro = m_engine->getDROManager();
    LODManager* lod = m_engine->getLODManager();
    
    // Get optimal radix for this distance
    RadixBase actualRadix = dro->getOptimalRadix(test.distance);
    
    // Get LOD settings
    auto lodSettings = lod->getLODSettings(test.distance);
    
    // Simulate precision measurement
    const PrecisionLevel* level = dro->getPrecisionLevel(test.distance);
    if (level) {
        // In real test, this would measure actual computational error
        test.measuredError = level->maxError * 0.5f; // Simulated: half of max error
        test.passed = (actualRadix == test.expectedRadix) && 
                      (test.measuredError <= level->maxError);
    } else {
        test.passed = false;
        test.measuredError = 999.9f;
    }
    
    // Log test result
    std::cout << "Test " << (index + 1) << ": Distance=" << test.distance << "m, "
              << "Expected=" << static_cast<int>(test.expectedRadix) << ", "
              << "Got=" << static_cast<int>(actualRadix) << ", "
              << "LOD=" << static_cast<int>(lodSettings.level) << ", "
              << "Error=" << test.measuredError << "m, "
              << (test.passed ? "PASS" : "FAIL") << std::endl;
    
    // Record to database
    DatabaseManager* db = m_engine->getDatabase();
    if (db && db->isOpen()) {
        PrecisionTestRecord record;
        record.timestamp = std::chrono::duration<double>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        record.demoName = getName();
        record.distance = test.distance;
        
        // Convert radix to string
        switch (actualRadix) {
            case RadixBase::BASE_4: record.radixUsed = "BASE_4"; break;
            case RadixBase::BASE_8: record.radixUsed = "BASE_8"; break;
            case RadixBase::BASE_16: record.radixUsed = "BASE_16"; break;
            case RadixBase::BASE_32: record.radixUsed = "BASE_32"; break;
            case RadixBase::BASE_64: record.radixUsed = "BASE_64"; break;
        }
        
        record.measuredError = test.measuredError;
        record.passed = test.passed;
        
        db->recordPrecisionTest(record);
    }
}

void PrecisionTestDemo::logResults() {
    int passCount = 0;
    for (const auto& test : m_testObjects) {
        if (test.passed) passCount++;
    }
    
    float passRate = (passCount * 100.0f) / m_testObjects.size();
    
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Total tests: " << m_testObjects.size() << std::endl;
    std::cout << "Passed: " << passCount << std::endl;
    std::cout << "Failed: " << (m_testObjects.size() - passCount) << std::endl;
    std::cout << "Pass rate: " << passRate << "%" << std::endl;
    
    // Get DRO metrics
    if (m_engine && m_engine->getDROManager()) {
        auto metrics = m_engine->getDROManager()->getMetrics();
        std::cout << "\nDRO Metrics:" << std::endl;
        std::cout << "Total radix switches: " << metrics.radixSwitches << std::endl;
        std::cout << "Average error: " << metrics.averageError << "m" << std::endl;
    }
}

} // namespace Demos
} // namespace BaseX
