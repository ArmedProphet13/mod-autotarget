#include "GameInterface/Hooks/FrameHook.h"

#include <windows.h>

#include "MinHook.h"

#include "Diagnostics/Logger.h"
#include "GameInterface/Memory.h"
#include "GameInterface/Offsets.h"

namespace autotarget {

namespace {

using EndSceneFn = HRESULT(__stdcall*)(void* device);

EndSceneFn          g_originalEndScene = nullptr;
FrameHook::Callback g_onFrame;
void*               g_endSceneAddr = nullptr;
void*               g_device = nullptr;

HRESULT __stdcall HookedEndScene(void* device) {
    g_device = device; // the live D3D9 device, captured on the game thread
    if (g_onFrame)
        g_onFrame();
    return g_originalEndScene(device);
}

// device = *(*(kD3DPtr1) + kD3DPtr2); EndScene = (*device)[vtable index 42].
void* ResolveEndScene() {
    const std::uintptr_t p1 = mem::ReadPtr(offsets::kD3DPtr1);
    if (p1 == 0)
        return nullptr;
    const std::uintptr_t device = mem::ReadPtr(p1 + offsets::kD3DPtr2);
    if (device == 0)
        return nullptr;
    const std::uintptr_t vtable = mem::ReadPtr(device);
    if (vtable == 0)
        return nullptr;
    return reinterpret_cast<void*>(mem::ReadPtr(vtable + offsets::kEndSceneVTableByte));
}

} // namespace

bool FrameHook::Install(Callback onFrame) {
    g_endSceneAddr = ResolveEndScene();
    if (g_endSceneAddr == nullptr) {
        AT_LOG_ERROR("FrameHook: could not resolve EndScene address");
        return false;
    }

    g_onFrame = std::move(onFrame);

    if (MH_CreateHook(g_endSceneAddr, &HookedEndScene,
                      reinterpret_cast<void**>(&g_originalEndScene)) != MH_OK) {
        AT_LOG_ERROR("FrameHook: MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(g_endSceneAddr) != MH_OK) {
        AT_LOG_ERROR("FrameHook: MH_EnableHook failed");
        return false;
    }

    AT_LOG_INFO("FrameHook: EndScene hooked at %p", g_endSceneAddr);
    return true;
}

void FrameHook::Uninstall() {
    g_onFrame = nullptr;
}

void* FrameHook::Device() {
    return g_device;
}

} // namespace autotarget
