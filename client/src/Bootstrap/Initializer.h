#pragma once

#include <windows.h>

namespace autotarget {

// Owns AutoTarget's lifecycle inside WoW.exe.
//
// Begin() is called from DllMain and returns immediately; the real work runs on
// a dedicated thread so nothing heavy happens under the loader lock. That thread
// loads config, verifies the client build, initializes MinHook, and installs the
// frame and selection hooks once the D3D device exists.
namespace Initializer {

// Spawns the initialization thread. `self` is this DLL's module handle, used to
// locate the WoW directory (the DLL ships next to WoW.exe).
void Begin(HMODULE self);

// Tears down on DLL detach. `processTerminating` is true when the whole process
// is exiting (the common case for a proxy DLL), in which case teardown is kept
// minimal — the OS reclaims everything.
void End(bool processTerminating);

} // namespace Initializer
} // namespace autotarget
