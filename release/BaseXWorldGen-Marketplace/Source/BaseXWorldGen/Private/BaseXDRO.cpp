#include <BaseX/DRO.h>

namespace BaseX {

DROManager::DROManager() {
    m_metrics = {0, 0, 0.0f, RadixBase::BASE_16};
}

void DROManager::initializeVRProfile() {
    m_precisionLevels.clear();
    m_precisionLevels.push_back(VRProfiles::HANDHELD);
    m_precisionLevels.push_back(VRProfiles::NEAR_FIELD);
    m_precisionLevels.push_back(VRProfiles::ROOM_SCALE);
    m_precisionLevels.push_back(VRProfiles::LARGE_SPACE);
    m_precisionLevels.push_back(VRProfiles::DISTANT);
    m_metrics.currentRadix = VRProfiles::HANDHELD.radix;
}

void DROManager::initializeCustomProfile(const std::vector<PrecisionLevel>& levels) {
    m_precisionLevels = levels;
}

RadixBase DROManager::getOptimalRadix(float distance) const {
    for (const auto& lvl : m_precisionLevels) {
        if (distance >= lvl.minDistance && distance < lvl.maxDistance) {
            return lvl.radix;
        }
    }
    return m_precisionLevels.empty() ? RadixBase::BASE_16 : m_precisionLevels.back().radix;
}

const PrecisionLevel* DROManager::getPrecisionLevel(float distance) const {
    for (const auto& lvl : m_precisionLevels) {
        if (distance >= lvl.minDistance && distance < lvl.maxDistance) {
            return &lvl;
        }
    }
    return nullptr;
}

bool DROManager::validatePrecision(float distance, float requiredAccuracy) const {
    auto lvl = getPrecisionLevel(distance);
    if (!lvl) return false;
    return lvl->maxError <= requiredAccuracy;
}

double DROManager::convertRadix(double value, RadixBase, RadixBase) const {
    return value;
}

void DROManager::resetMetrics() {
    m_metrics = {0, 0, 0.0f, m_metrics.currentRadix};
}

double DROManager::toRadix(double value, RadixBase) const {
    return value;
}

double DROManager::fromRadix(double value, RadixBase) const {
    return value;
}

LODManager::LODManager(DROManager* droManager) : m_droManager(droManager) {
    m_lodThresholds = {0.5f, 2.0f, 10.0f, 100.0f};
}

LODManager::LODLevel LODManager::getLODLevel(float distance) const {
    if (distance < m_lodThresholds[0]) return LODLevel::LOD0_HIGHEST;
    if (distance < m_lodThresholds[1]) return LODLevel::LOD1_HIGH;
    if (distance < m_lodThresholds[2]) return LODLevel::LOD2_MEDIUM;
    if (distance < m_lodThresholds[3]) return LODLevel::LOD3_LOW;
    return LODLevel::LOD4_LOWEST;
}

LODManager::LODSettings LODManager::getLODSettings(float distance) const {
    LODSettings s;
    s.distance = distance;
    s.level = getLODLevel(distance);
    switch (s.level) {
        case LODLevel::LOD0_HIGHEST:
            s.radix = RadixBase::BASE_64;
            s.polyReduction = 0.0f;
            s.enablePhysics = true;
            break;
        case LODLevel::LOD1_HIGH:
            s.radix = RadixBase::BASE_32;
            s.polyReduction = 0.1f;
            s.enablePhysics = true;
            break;
        case LODLevel::LOD2_MEDIUM:
            s.radix = RadixBase::BASE_16;
            s.polyReduction = 0.25f;
            s.enablePhysics = true;
            break;
        case LODLevel::LOD3_LOW:
            s.radix = RadixBase::BASE_8;
            s.polyReduction = 0.5f;
            s.enablePhysics = false;
            break;
        default:
            s.radix = RadixBase::BASE_4;
            s.polyReduction = 0.75f;
            s.enablePhysics = false;
            break;
    }
    return s;
}

void LODManager::setLODThresholds(const std::vector<float>& thresholds) {
    m_lodThresholds = thresholds;
}

} // namespace BaseX
