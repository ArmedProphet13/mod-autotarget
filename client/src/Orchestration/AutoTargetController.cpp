#include "Orchestration/AutoTargetController.h"

namespace autotarget {

AutoTargetController::AutoTargetController(const ConfigManager& config)
    : toggle_(config.Get().enabledOnStartup),
      tick_(config, toggle_) {}

} // namespace autotarget
