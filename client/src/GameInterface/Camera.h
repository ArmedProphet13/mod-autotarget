#pragma once

#include "GameInterface/WoWObject.h"

namespace autotarget {

// Supplies the aim yaw used by the cone math.
//
// Ideally this is the camera yaw (what the player is looking at). The camera
// offset chain is single-sourced and unverified, so v1 falls back to the local
// player's model facing, which is reliable and, in melee, closely tracks the
// camera. Once the camera chain is confirmed in-client, flip kCameraChainVerified
// in Camera.cpp.
class Camera {
public:
    // Aim yaw in radians (atan2 convention). Uses the camera when available,
    // otherwise the player's model facing.
    static float AimYaw(const WoWUnit& localPlayer);

    // Attempts the camera-struct yaw read. Returns false when unavailable.
    static bool TryReadCameraYaw(float& outYaw);

    // Dumps the model facing and the Direct3D view matrix to the debug log, so
    // the camera orientation can be recovered offset-free. Temporary tooling.
    static void LogProbe(const WoWUnit& localPlayer, void* d3dDevice);
};

} // namespace autotarget
