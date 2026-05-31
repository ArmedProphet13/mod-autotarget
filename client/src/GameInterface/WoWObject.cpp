#include "GameInterface/WoWObject.h"

namespace autotarget {

bool WoWUnit::IsAttackable() const {
    if (!IsUnit() || !IsAlive())
        return false;

    const std::uint32_t f = Flags();
    if (f & offsets::kUnitFlagNonAttackable)  return false;
    if (f & offsets::kUnitFlagNotSelectable)  return false;
    if (f & offsets::kUnitFlagImmuneToPlayer) return false;

    return true;
}

} // namespace autotarget
