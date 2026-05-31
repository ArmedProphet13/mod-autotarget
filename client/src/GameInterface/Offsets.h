#pragma once

#include <cstdint>

// ============================================================================
// WoW 3.3.5a (build 12340) client offsets.
//
// This file is the SINGLE SOURCE OF TRUTH for every offset-bound constant. No
// other file in the project hard-codes a client address. If the deployed client
// ever differs, this is the only file to touch.
//
// The 12340 client has been frozen since 2010, so these constants are stable.
// They are grouped by confidence:
//
//   [CONFIRMED] - cross-verified across multiple independent public sources
//                 and, for descriptor indices, against AzerothCore's
//                 UpdateFields.h (the authoritative protocol definition).
//   [VERIFY]    - published but sourced from a single reference, or sources
//                 disagree. Must be confirmed in-client before relying on it.
//                 Startup signature checks and in-client testing will catch a
//                 wrong value; see Initializer / the README.
//
// Sources: community memory-editing references for 3.3.5a 12340 and the
// AzerothCore UpdateFields.h descriptor table.
// ============================================================================

namespace autotarget {
namespace offsets {

// ---- Direct3D 9 EndScene hook [CONFIRMED] ----------------------------------
// Device is reached as *(*(D3D_PTR_1) + D3D_PTR_2). EndScene is vtable index 42.
constexpr std::uintptr_t kD3DPtr1            = 0x00C5DF88;
constexpr std::uintptr_t kD3DPtr2            = 0x0000397C; // device = *(*(kD3DPtr1) + kD3DPtr2)
constexpr std::uintptr_t kEndSceneVTableByte = 0x000000A8; // vtable index 42 * 4

// ---- Object manager [CONFIRMED] --------------------------------------------
constexpr std::uintptr_t kStaticClientConnection = 0x00C79CE0;
constexpr std::uintptr_t kObjectManagerOffset    = 0x00002ED0; // ClientConnection -> ObjMgr
constexpr std::uintptr_t kFirstObjectOffset      = 0x000000AC; // ObjMgr -> first object
constexpr std::uintptr_t kLocalGuidOffset        = 0x000000C0; // ObjMgr -> local player GUID
constexpr std::uintptr_t kNextObjectOffset       = 0x0000003C; // object -> next object

// ---- Object struct: identity [CONFIRMED] -----------------------------------
constexpr std::uintptr_t kObjTypeOffset       = 0x00000014; // object base -> type id (byte)
constexpr std::uintptr_t kObjGuidOffset       = 0x00000030; // object base -> GUID (uint64)
constexpr std::uintptr_t kObjDescriptorOffset = 0x00000008; // object base -> descriptor pointer

// ---- Object struct: position [CONFIRMED via behavioural diagnosis] ---------
// Canonical 12340 CGUnit_C layout: a contiguous {X, Y, Z} float triple at
// 0x798, with the orientation (facing) at 0x7A8. An earlier swap (X<->Y) made
// atan2(dy,dx) compute the angle from +Y instead of +X, while the facing read
// remained in the true +X convention - the resulting mismatch produced picks
// offset to the player's left or right depending on which direction they were
// facing, which is exactly the symptom we observed. Distances were unaffected
// because |(dx,dy)| is independent of axis labels.
constexpr std::uintptr_t kUnitPosXOffset    = 0x00000798;
constexpr std::uintptr_t kUnitPosYOffset    = 0x0000079C;
constexpr std::uintptr_t kUnitPosZOffset    = 0x000007A0;
constexpr std::uintptr_t kUnitRotationOffset = 0x000007A8; // model facing, radians

// ---- Descriptor field byte offsets [CONFIRMED] -----------------------------
// Index (from AzerothCore UpdateFields.h, OBJECT_END = 0x6) multiplied by 4.
// Relative to the descriptor pointer at kObjDescriptorOffset.
constexpr std::uintptr_t kDescGuid            = 0x00000000; // OBJECT_FIELD_GUID         (idx 0x00)
constexpr std::uintptr_t kDescType            = 0x00000008; // OBJECT_FIELD_TYPE         (idx 0x02)
constexpr std::uintptr_t kUnitFieldTarget     = 0x00000048; // UNIT_FIELD_TARGET         (idx 0x12)
constexpr std::uintptr_t kUnitFieldHealth     = 0x00000060; // UNIT_FIELD_HEALTH         (idx 0x18)
constexpr std::uintptr_t kUnitFieldMaxHealth  = 0x00000080; // UNIT_FIELD_MAXHEALTH      (idx 0x20)
constexpr std::uintptr_t kUnitFieldLevel      = 0x000000D8; // UNIT_FIELD_LEVEL          (idx 0x36)
constexpr std::uintptr_t kUnitFieldFaction    = 0x000000DC; // UNIT_FIELD_FACTIONTEMPLATE(idx 0x37)
constexpr std::uintptr_t kUnitFieldFlags      = 0x000000EC; // UNIT_FIELD_FLAGS          (idx 0x3B)
constexpr std::uintptr_t kUnitDynamicFlags    = 0x0000013C; // UNIT_DYNAMIC_FLAGS        (idx 0x4F)

// ---- Static player/target state [VERIFY] -----------------------------------
// The local target GUID is also derivable from the unit descriptor; the static
// is a convenience. Sources cite 0xBD07A0 and 0xBD07B0 - confirm in-client.
constexpr std::uintptr_t kStaticTargetGuid    = 0x00BD07B0;
constexpr std::uintptr_t kStaticMouseoverGuid = 0x00BD07A0;

// ---- Functions [CONFIRMED unless noted] ------------------------------------
// FrameScript_Execute(code, source, 0): runs a Lua string. Used to install the
// Interface Options checkbox. Called as void(__cdecl*)(const char*, const char*, int).
constexpr std::uintptr_t kFnFrameScriptExecute = 0x00819210;

// FrameScript_RegisterFunction(name, fnptr): exposes a native function to Lua.
// Required for the Interface Options > Combat > "Enable AutoTarget" checkbox
// and the /at chat command: without the bridge, Lua can't call back into the
// DLL to flip the enable state.
//
// [VERIFY] - single source. The call site is wrapped in SEH (see FrameScript.cpp);
// if this offset is wrong the first call faults, the bridge is latched off,
// the checkbox is not installed, and a warning appears in the log. The mod
// remains functional in its default-enabled state.
constexpr std::uintptr_t kFnFrameScriptRegisterFunction = 0x00817F90;

// CGGameUI::Target(guid): sets the player's selection. [VERIFY] - single source.
constexpr std::uintptr_t kFnTargetUnit = 0x00524BF0;

// Spell_C_CastSpell(spellId, unused, targetGuid, unk): the client's spell-cast
// entry point. AutoTarget detours this in ActionTarget mode so it can inject
// the engine's soft pick as the hard target *before* the client's no-target
// check, giving plain /cast macros Action-Targeting behaviour. [VERIFY] -
// single source; a wrong address makes MinHook install fail and the mod
// silently falls back to Mouseover behaviour.
constexpr std::uintptr_t kFnSpellCastSpell = 0x0080DA40;

// CGGameUI::Set_Mouseover(guid): the client's mouseover entry point. Unlike a
// raw write to kStaticMouseoverGuid (which only sets the slot read by
// [target=mouseover] macros), this routine also opens the tooltip and lights
// up the yellow selection-ring highlight. Calling it every frame with the
// engine's soft pick is what gives the player constant visual feedback on
// the AutoTarget aim pick.
//
// [VERIFY] - single source. If the address is wrong, Selection::SetMouseover
// catches the fault with SEH on the first call and permanently falls back to
// the raw slot write (functional macros, no highlight). Set to 0 to disable
// the call entirely and keep raw-write-only behaviour.
constexpr std::uintptr_t kFnSetMouseover = 0x004F62E0;

// CWorld::Intersect(start, end, hitOut, distOut, flags): the client's terrain /
// WMO / doodad ray tracer. This is the routine the client itself uses to gate
// "Target not in line of sight" before a cast. AutoTarget calls it (tick-only,
// never on the cast hook) to drop occluded enemies from candidate scoring and
// to activate the SmartUnstick OutOfLoS reason.
//
// Signature:  char __cdecl Intersect(const C3Vector* start, const C3Vector* end,
//                                     C3Vector* hitOut, float* distOut,
//                                     unsigned int flags);
//   - C3Vector is three contiguous floats {x, y, z} (z = up), matching our unit
//     position read order at kUnitPosX/Y/ZOffset.
//   - distOut is in/out: initialise to 1.0f; on a hit it is the [0,1] fraction
//     along the segment where geometry was struck. hitOut may be null.
//   - RETURN IS INVERTED: nonzero (1) means the ray HIT geometry (blocked / NOT
//     in line of sight); zero means the segment is clear (in line of sight).
//
// [VERIFY] - the offset is stable across public 12340 references; the flag
// bitmask below is the part most likely to need an in-client tweak. The call is
// SEH-wrapped in LineOfSight.cpp and latches off on first fault, falling back to
// the permissive (always-visible) behaviour, so a wrong value cannot crash or
// regress targeting.
constexpr std::uintptr_t kFnWorldIntersect = 0x007A3B70;

// Collision mask for a line-of-sight trace: terrain + WMO (buildings) + M2
// (doodads). The widely-cited LoS combination for 12340. [VERIFY] in-client.
constexpr unsigned int kLosTraceFlags = 0x00100171;

// TargetNearestEnemy: the native Tab handler. AutoTarget detours this so
// Tab targets the engine's aim pick (the visible mouseover ring) instead of
// cycling enemies by client-side proximity. If MinHook install fails Tab
// keeps its native behaviour and the mod logs a warning. [VERIFY] -
// single source; community references give addresses in the 0x004FE0..F000
// range for the Script_*/CGUnit_C__TargetNearestEnemy family.
constexpr std::uintptr_t kFnTargetNearestEnemy = 0x004FE490;

// ---- Camera [VERIFY] -------------------------------------------------------
// Single-source chain: *(*(*(kCameraBasePtr) + kCameraOff1) + kCameraOff2).
// Yaw is a float at kCameraYawOffset within the camera struct. Until verified
// in-client, the Camera class falls back to the player's model facing
// (kUnitRotationOffset), which is reliable and, in melee, a close match.
constexpr std::uintptr_t kCameraBasePtr   = 0x00C7B5A8;
constexpr std::uintptr_t kCameraOff1      = 0x00006B04;
constexpr std::uintptr_t kCameraOff2      = 0x000000E8;
constexpr std::uintptr_t kCameraYawOffset = 0x00000030;

// ---- Object type ids -------------------------------------------------------
constexpr std::uint8_t kTypeUnit   = 3;
constexpr std::uint8_t kTypePlayer = 4;

// ---- UNIT_FIELD_FLAGS bits -------------------------------------------------
constexpr std::uint32_t kUnitFlagNonAttackable  = 0x00000002;
constexpr std::uint32_t kUnitFlagImmuneToPlayer = 0x00000100;
constexpr std::uint32_t kUnitFlagInCombat       = 0x00080000;
constexpr std::uint32_t kUnitFlagNotSelectable  = 0x02000000;

} // namespace offsets
} // namespace autotarget
