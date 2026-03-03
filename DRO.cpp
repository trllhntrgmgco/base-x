#include "BaseX/DRO.h"
#include <algorithm>
#include <cstring>

namespace BaseX {

DROManager::DROManager() {
    std::memset(&m_metrics, 0, sizeof(Metrics));
    m_metrics.currentRadix = RadixBase::BASE_16;
}

void DROManager::initializeVRProfile() {
    m_precisionLevels = {
        VRProfiles::HANDHELD,
        VRProfiles::NEAR_FIELD,
        VRProfiles::ROOM_SCALE,
        VRProfiles::LARGE_SPACE,
        VRProfiles::DISTANT
    };
    
    // Sort by distance
    std::sort(m_precisionLevels.begin(), m_precisionLevels.end(),
        [](const PrecisionLevel& a, const PrecisionLevel& b) {
            return a.minDistance < b.minDistance;
        });
}

void DROManager::initializeCustomProfile(const std::vector<PrecisionLevel>& levels) {
    m_precisionLevels = levels;
    std::sort(m_precisionLevels.begin(), m_precisionLevels.end(),
        [](const PrecisionLevel& a, const PrecisionLevel& b) {
            return a.minDistance < b.minDistance;
        });
}

RadixBase DROManager::getOptimalRadix(float distance) const {
    const PrecisionLevel* level = getPrecisionLevel(distance);
    if (level) {
        // Track radix switches for metrics
        if (level->radix != m_metrics.currentRadix) {
            m_metrics.radixSwitches++;
            m_metrics.currentRadix = level->radix;
        }
        return level->radix;
    }
    return RadixBase::BASE_16; // Default fallback
}

const PrecisionLevel* DROManager::getPrecisionLevel(float distance) const {
    float absDist = std::abs(distance);
    
    for (const auto& level : m_precisionLevels) {
        if (absDist >= level.minDistance && absDist < level.maxDistance) {
            return &level;
        }
    }
    
    // Return furthest level if beyond all ranges
    if (!m_precisionLevels.empty()) {
        return &m_precisionLevels.back();
    }
    
    return nullptr;
}

bool DROManager::validatePrecision(float distance, float requiredAccuracy) const {
    const PrecisionLevel* level = getPrecisionLevel(distance);
    if (!level) {
        m_metrics.precisionWarnings++;
        return false;
    }
    
    bool valid = level->maxError <= requiredAccuracy;
    if (!valid) {
        m_metrics.precisionWarnings++;
    }
    
    return valid;
}

double DROManager::convertRadix(double value, RadixBase fromBase, RadixBase toBase) const {
    if (fromBase == toBase) {
        return value;
    }
    
    // Convert through decimal intermediate
    double decimal = fromRadix(value, fromBase);
    return toRadix(decimal, toBase);
}

double DROManager::toRadix(double value, RadixBase base) const {
    // Convert decimal to specified radix representation
    // This is a simplified conversion - full implementation would handle
    // mantissa/exponent representation in the target base
    
    int baseValue = static_cast<int>(base);
    if (baseValue == 16) {
        return value; // Base-16 is native for hexadecimal
    }
    
    // For other bases, apply precision scaling
    double scale = std::log2(baseValue) / std::log2(16.0);
    return value * scale;
}

double DROManager::fromRadix(double value, RadixBase base) const {
    // Convert from specified radix to decimal
    int baseValue = static_cast<int>(base);
    if (baseValue == 16) {
        return value;
    }
    
    double scale = std::log2(16.0) / std::log2(baseValue);
    return value * scale;
}

void DROManager::resetMetrics() {
    std::memset(&m_metrics, 0, sizeof(Metrics));
    m_metrics.currentRadix = RadixBase::BASE_16;
}

// LODManager Implementation

LODManager::LODManager(DROManager* droManager) 
    : m_droManager(droManager) {
    // Default VR LOD thresholds
    m_lodThresholds = {
        0.5f,   // LOD0: 0-0.5m
        2.0f,   // LOD1: 0.5-2m
        10.0f,  // LOD2: 2-10m
        50.0f,  // LOD3: 10-50m
        200.0f  // LOD4: 50m+
    };
}

LODManager::LODLevel LODManager::getLODLevel(float distance) const {
    float absDist = std::abs(distance);
    
    for (size_t i = 0; i < m_lodThresholds.size(); ++i) {
        if (absDist < m_lodThresholds[i]) {
            return static_cast<LODLevel>(i);
        }
    }
    
    return LODLevel::LOD4_LOWEST;
}

LODManager::LODSettings LODManager::getLODSettings(float distance) const {
    LODSettings settings;
    settings.distance = distance;
    settings.level = getLODLevel(distance);
    
    if (m_droManager) {
        settings.radix = m_droManager->getOptimalRadix(distance);
    } else {
        settings.radix = RadixBase::BASE_16;
    }
    
    // Configure settings based on LOD level
    switch (settings.level) {
        case LODLevel::LOD0_HIGHEST:
            settings.polyReduction = 1.0f;
            settings.enablePhysics = true;
            break;
        case LODLevel::LOD1_HIGH:
            settings.polyReduction = 0.75f;
            settings.enablePhysics = true;
            break;
        case LODLevel::LOD2_MEDIUM:
            settings.polyReduction = 0.5f;
            settings.enablePhysics = true;
            break;
        case LODLevel::LOD3_LOW:
            settings.polyReduction = 0.25f;
            settings.enablePhysics = false;
            break;
        case LODLevel::LOD4_LOWEST:
            settings.polyReduction = 0.1f;
            settings.enablePhysics = false;
            break;
    }
    
    return settings;
}

void LODManager::setLODThresholds(const std::vector<float>& thresholds) {
    m_lodThresholds = thresholds;
    std::sort(m_lodThresholds.begin(), m_lodThresholds.end());
}

} // namespace BaseX
