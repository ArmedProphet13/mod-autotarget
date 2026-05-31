// Off-client tests for ToggleManager. SyncToLua calls into FrameScript
// (Windows-bound), so for the tests we stub FrameScript::Execute below.

#include <cstdio>

#include "Orchestration/ToggleManager.h"

extern int g_pass;
extern int g_fail;
#define TM_CHECK(cond)                                                         \
    do {                                                                       \
        if (cond) {                                                            \
            ++g_pass;                                                          \
        } else {                                                               \
            ++g_fail;                                                          \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                      \
    } while (0)

namespace {

void Test_Toggle_StartsAtConfigValue() {
    std::printf("Test_Toggle_StartsAtConfigValue\n");
    autotarget::ToggleManager on(true);
    TM_CHECK(on.IsEnabled() == true);
    autotarget::ToggleManager off(false);
    TM_CHECK(off.IsEnabled() == false);
}

void Test_Toggle_SetEnabledFlips() {
    std::printf("Test_Toggle_SetEnabledFlips\n");
    autotarget::ToggleManager t(false);
    t.SetEnabled(true);
    TM_CHECK(t.IsEnabled() == true);
    t.SetEnabled(false);
    TM_CHECK(t.IsEnabled() == false);
}

void Test_Toggle_RedundantSetIsIdempotent() {
    std::printf("Test_Toggle_RedundantSetIsIdempotent\n");
    autotarget::ToggleManager t(true);
    t.SetEnabled(true);
    TM_CHECK(t.IsEnabled() == true);
}

} // namespace

void RunToggleManagerTests() {
    Test_Toggle_StartsAtConfigValue();
    Test_Toggle_SetEnabledFlips();
    Test_Toggle_RedundantSetIsIdempotent();
}
