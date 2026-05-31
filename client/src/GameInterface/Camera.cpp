#include "GameInterface/Camera.h"

#include <cmath>

#include "Diagnostics/Logger.h"
#include "GameInterface/Memory.h"
#include "GameInterface/Offsets.h"

namespace autotarget {

namespace {
// Set true once the camera offset chain in Offsets.h is confirmed in-client.
constexpr bool kCameraChainVerified = false;
} // namespace

bool Camera::TryReadCameraYaw(float& outYaw) {
    if (!kCameraChainVerified)
        return false;

    const std::uintptr_t p1 = mem::ReadPtr(offsets::kCameraBasePtr);
    if (p1 == 0)
        return false;
    const std::uintptr_t p2 = mem::ReadPtr(p1 + offsets::kCameraOff1);
    if (p2 == 0)
        return false;
    const std::uintptr_t cam = mem::ReadPtr(p2 + offsets::kCameraOff2);
    if (cam == 0)
        return false;

    const float yaw = mem::ReadOr<float>(cam + offsets::kCameraYawOffset, 0.0f);
    if (!std::isfinite(yaw))
        return false;

    outYaw = yaw;
    return true;
}

float Camera::AimYaw(const WoWUnit& localPlayer) {
    float yaw = 0.0f;
    if (TryReadCameraYaw(yaw))
        return yaw;
    return localPlayer.Facing();
}

void Camera::LogProbe(const WoWUnit& localPlayer, void* d3dDevice) {
    AT_LOG_DEBUG("camprobe: modelFacing=%.4f", localPlayer.Facing());
    if (d3dDevice == nullptr) {
        AT_LOG_DEBUG("camprobe: no D3D device yet");
        return;
    }

    // IDirect3DDevice9::GetTransform is vtable index 45; D3DTS_VIEW is 2.
    void** vtable = *reinterpret_cast<void***>(d3dDevice);
    using GetTransformFn = long(__stdcall*)(void*, unsigned, float*);
    const auto getTransform = reinterpret_cast<GetTransformFn>(vtable[45]);

    float m[16] = {0.0f};
    const long hr = getTransform(d3dDevice, 2, m);
    AT_LOG_DEBUG("viewmat: hr=%ld", hr);
    AT_LOG_DEBUG("viewmat: %9.4f %9.4f %9.4f %9.4f", m[0],  m[1],  m[2],  m[3]);
    AT_LOG_DEBUG("viewmat: %9.4f %9.4f %9.4f %9.4f", m[4],  m[5],  m[6],  m[7]);
    AT_LOG_DEBUG("viewmat: %9.4f %9.4f %9.4f %9.4f", m[8],  m[9],  m[10], m[11]);
    AT_LOG_DEBUG("viewmat: %9.4f %9.4f %9.4f %9.4f", m[12], m[13], m[14], m[15]);
}

} // namespace autotarget
