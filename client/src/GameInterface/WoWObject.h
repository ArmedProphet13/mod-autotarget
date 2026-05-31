#pragma once

#include <cstdint>

#include "Engine/EngineTypes.h"
#include "GameInterface/Memory.h"
#include "GameInterface/Offsets.h"

namespace autotarget {

// Lightweight wrapper around a live client object's base address. Copyable and
// cheap — it holds only the base pointer and reads through to client memory on
// demand.
class WoWObject {
public:
    WoWObject() = default;
    explicit WoWObject(std::uintptr_t base) : base_(base) {}

    bool Valid() const { return base_ != 0; }
    std::uintptr_t Base() const { return base_; }

    std::uint8_t TypeId() const {
        return mem::ReadOr<std::uint8_t>(base_ + offsets::kObjTypeOffset, 0);
    }
    bool IsUnit() const {
        const std::uint8_t t = TypeId();
        return t == offsets::kTypeUnit || t == offsets::kTypePlayer;
    }
    bool IsPlayer() const { return TypeId() == offsets::kTypePlayer; }

    Guid ObjectGuid() const {
        return mem::ReadOr<Guid>(base_ + offsets::kObjGuidOffset, kNoGuid);
    }

protected:
    std::uintptr_t base_ = 0;

    std::uintptr_t Descriptor() const {
        return mem::ReadPtr(base_ + offsets::kObjDescriptorOffset);
    }
};

// A unit (creature or player) with the combat-relevant accessors the engine
// snapshot needs.
class WoWUnit : public WoWObject {
public:
    WoWUnit() = default;
    explicit WoWUnit(std::uintptr_t base) : WoWObject(base) {}

    float X() const { return mem::ReadOr<float>(base_ + offsets::kUnitPosXOffset, 0.0f); }
    float Y() const { return mem::ReadOr<float>(base_ + offsets::kUnitPosYOffset, 0.0f); }
    float Z() const { return mem::ReadOr<float>(base_ + offsets::kUnitPosZOffset, 0.0f); }
    float Facing() const { return mem::ReadOr<float>(base_ + offsets::kUnitRotationOffset, 0.0f); }

    std::uint32_t Health() const     { return DescField<std::uint32_t>(offsets::kUnitFieldHealth); }
    std::uint32_t MaxHealth() const  { return DescField<std::uint32_t>(offsets::kUnitFieldMaxHealth); }
    std::uint32_t Flags() const      { return DescField<std::uint32_t>(offsets::kUnitFieldFlags); }
    std::uint32_t DynamicFlags() const { return DescField<std::uint32_t>(offsets::kUnitDynamicFlags); }
    std::uint32_t Faction() const    { return DescField<std::uint32_t>(offsets::kUnitFieldFaction); }
    std::uint32_t Level() const      { return DescField<std::uint32_t>(offsets::kUnitFieldLevel); }
    Guid TargetGuid() const          { return DescField<Guid>(offsets::kUnitFieldTarget); }

    bool IsAlive() const { return Health() > 0; }

    float HealthPercent() const {
        const std::uint32_t mx = MaxHealth();
        return mx == 0 ? 0.0f : static_cast<float>(Health()) / static_cast<float>(mx);
    }

    // Best-effort critter guess: critters carry a tiny max-health pool. This is
    // a cheap secondary guard; the real exclusion of non-enemies is IsAttackable.
    bool IsCritter() const {
        const std::uint32_t mx = MaxHealth();
        return mx > 0 && mx <= 15 && !IsPlayer();
    }

    // Whether the local player may attack this unit.
    //
    // v1 checks type, alive state, and the not-attackable / not-selectable /
    // immune flags. It does NOT yet distinguish hostile creatures from friendly
    // NPCs — that needs the client's faction-reaction routine, a documented
    // in-client verification item. The Orchestration's commit-on-combat gate
    // keeps this gap from mattering while the player is just walking around.
    bool IsAttackable() const;

private:
    template <typename T>
    T DescField(std::uintptr_t fieldByteOffset) const {
        const std::uintptr_t desc = Descriptor();
        if (desc == 0)
            return T{};
        return mem::ReadOr<T>(desc + fieldByteOffset, T{});
    }
};

} // namespace autotarget
