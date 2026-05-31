#include "GameInterface/LineOfSight.h"

namespace autotarget {
namespace LineOfSight {

bool Visible(const WoWUnit& /*from*/, const WoWUnit& /*to*/) {
    // v1: permissive. See the header for the rationale and the upgrade path.
    return true;
}

} // namespace LineOfSight
} // namespace autotarget
