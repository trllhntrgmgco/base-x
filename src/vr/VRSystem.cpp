#include "../include/BaseX/VRSystem.h"
#include <iostream>

namespace BaseX {

struct VRSystem::Impl {
    bool running = false;
    bool focused = false;
    VRConfig config;
    VRSessionState state = VRSessionState::Unknown;
    VRSessionStateCallback callback;
};

VRSystem::VRSystem() : m_impl(std::make_unique<Impl>()) {}
VRSystem::~VRSystem() { shutdown(); }
VRSystem::VRSystem(VRSystem&& other) noexcept = default;
VRSystem& VRSystem::operator=(VRSystem&&) noexcept = default;

bool VRSystem::initialize(VkInstance, VkPhysicalDevice, VkDevice, VkQueue, uint32_t, const VRConfig& config) {
    m_impl->config = config;
    m_impl->running = false; // not actually initialized (stub)
    m_impl->state = VRSessionState::Idle;
    m_impl->focused = false;
    std::cerr << "VRSystem: OpenXR not linked. This is a stub implementation.\n";
    return true;
}

void VRSystem::shutdown() {
    if (m_impl) {
        m_impl->running = false;
        m_impl->state = VRSessionState::Exiting;
    }
}

bool VRSystem::isRunning() const { return m_impl && m_impl->running; }
bool VRSystem::isFocused() const { return m_impl && m_impl->focused; }

void VRSystem::setSessionStateCallback(VRSessionStateCallback cb) { if (m_impl) m_impl->callback = cb; }

VRFrameState VRSystem::beginFrame() { return VRFrameState{}; }
VREyeTarget VRSystem::getEyeTarget(int) { return VREyeTarget{}; }
void VRSystem::releaseEyeTarget(int) {}
void VRSystem::endFrame() {}
VRInputState VRSystem::getInputState() const { return VRInputState{}; }
void VRSystem::triggerHaptic(int, float, float, float) {}
void VRSystem::getRecommendedResolution(uint32_t& w, uint32_t& h) const { w = 1024; h = 1024; }
VRSessionState VRSystem::getSessionState() const { return m_impl ? m_impl->state : VRSessionState::Unknown; }
std::string VRSystem::getRuntimeName() const { return "StubRuntime"; }
std::string VRSystem::getSystemName() const { return "StubSystem"; }
uint32_t VRSystem::getSwapchainImageCount() const { return 1; }
std::vector<VkImage> VRSystem::getSwapchainImages(int) const { return {}; }
VkFormat VRSystem::getSwapchainFormat() const { return 0; }

} // namespace BaseX
