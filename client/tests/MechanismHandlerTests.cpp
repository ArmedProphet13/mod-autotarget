// Off-client tests for IMechanismHandler implementations.
//
// Each handler depends only on ISelectionSink + MechanismCtx + Engine
// types. We mock the sink, hand in canned context + decision, and assert
// the writes the handler chose. Logger is stubbed below since
// HardTargetHandler logs on commit.

#include <cstdio>
#include <vector>

#include "Engine/Mechanisms/ActionTargetHandler.h"
#include "Engine/Mechanisms/HardTargetHandler.h"
#include "Engine/Mechanisms/MouseoverHandler.h"
#include "Interfaces/ISelectionSink.h"

using namespace autotarget;

extern int g_pass;
extern int g_fail;
#define MH_CHECK(cond)                                                         \
    do {                                                                       \
        if (cond) {                                                            \
            ++g_pass;                                                          \
        } else {                                                               \
            ++g_fail;                                                          \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                      \
    } while (0)

namespace {

// Recording mock sink. Captures the write sequence so tests can assert it.
struct MockSink : ISelectionSink {
    Guid mouseoverFromClient = kNoGuid; // what ReadMouseover returns

    std::vector<Guid> mouseoverWrites;
    std::vector<Guid> activeTargetWrites;
    int beginCount = 0;
    int endCount   = 0;

    Guid ReadMouseover() override { return mouseoverFromClient; }
    void WriteMouseover(Guid g) override { mouseoverWrites.push_back(g); }
    void BeginActiveTargetCommit() override { ++beginCount; }
    void EndActiveTargetCommit() override { ++endCount; }
    void WriteActiveTarget(Guid g) override { activeTargetWrites.push_back(g); }
};

MechanismCtx Ctx(Guid soft, Guid lastMouseover = kNoGuid,
                 bool enabled = true, bool diagnostic = false) {
    MechanismCtx c{};
    c.softTarget = soft;
    c.lastWrittenMouseover = lastMouseover;
    c.enabled = enabled;
    c.diagnostic = diagnostic;
    return c;
}

void Test_Action_WritesSoftPickWhenCursorIdle() {
    std::printf("Test_Action_WritesSoftPickWhenCursorIdle\n");
    MockSink sink;
    ActionTargetHandler h(sink);
    auto ctx = Ctx(/*soft=*/0xAAAA);
    h.OnFrame(ctx);
    MH_CHECK(sink.mouseoverWrites.size() == 1);
    MH_CHECK(sink.mouseoverWrites[0] == 0xAAAA);
    MH_CHECK(ctx.lastWrittenMouseover == 0xAAAA);
}

void Test_Action_YieldsWhenCursorAsserts() {
    std::printf("Test_Action_YieldsWhenCursorAsserts\n");
    MockSink sink;
    sink.mouseoverFromClient = 0xBBBB; // cursor has a different unit hovered
    ActionTargetHandler h(sink);
    auto ctx = Ctx(/*soft=*/0xAAAA, /*lastMouseover=*/0xAAAA);
    h.OnFrame(ctx);
    MH_CHECK(sink.mouseoverWrites.empty()); // no write while cursor wins
}

void Test_Action_RepinsWhenClientClearedSlot() {
    std::printf("Test_Action_RepinsWhenClientClearedSlot\n");
    MockSink sink;
    sink.mouseoverFromClient = kNoGuid; // client cleared after brief hover
    ActionTargetHandler h(sink);
    auto ctx = Ctx(/*soft=*/0xAAAA, /*lastMouseover=*/0xAAAA);
    h.OnFrame(ctx);
    MH_CHECK(sink.mouseoverWrites.size() == 1);
    MH_CHECK(sink.mouseoverWrites[0] == 0xAAAA);
}

void Test_Action_DiagnosticSuppresses() {
    std::printf("Test_Action_DiagnosticSuppresses\n");
    MockSink sink;
    ActionTargetHandler h(sink);
    auto ctx = Ctx(/*soft=*/0xAAAA, /*lastMouseover=*/0,
                   /*enabled=*/true, /*diagnostic=*/true);
    h.OnFrame(ctx);
    MH_CHECK(sink.mouseoverWrites.empty());
}

void Test_Action_DisabledSuppresses() {
    std::printf("Test_Action_DisabledSuppresses\n");
    MockSink sink;
    ActionTargetHandler h(sink);
    auto ctx = Ctx(/*soft=*/0xAAAA, /*lastMouseover=*/0,
                   /*enabled=*/false);
    h.OnFrame(ctx);
    MH_CHECK(sink.mouseoverWrites.empty());
}

void Test_Action_NeverWritesActiveTarget() {
    std::printf("Test_Action_NeverWritesActiveTarget\n");
    MockSink sink;
    ActionTargetHandler h(sink);
    TargetingDecision d{};
    d.kind = DecisionKind::SetHardTarget;
    d.hardTarget = 0xCCCC;
    auto ctx = Ctx(/*soft=*/0xAAAA);
    h.OnTickResult(ctx, d);
    MH_CHECK(sink.activeTargetWrites.empty());
    MH_CHECK(sink.beginCount == 0);
}

void Test_Mouseover_TickStoresDecision() {
    std::printf("Test_Mouseover_TickStoresDecision\n");
    MockSink sink;
    MouseoverHandler h(sink);
    TargetingDecision d{};
    d.softTarget = 0xDDDD;
    auto ctx = Ctx(/*soft=*/0);
    h.OnTickResult(ctx, d);
    MH_CHECK(ctx.lastWrittenMouseover == 0xDDDD);
}

void Test_Mouseover_FrameWritesCachedValue() {
    std::printf("Test_Mouseover_FrameWritesCachedValue\n");
    MockSink sink;
    MouseoverHandler h(sink);
    auto ctx = Ctx(/*soft=*/0, /*lastMouseover=*/0xDDDD);
    h.OnFrame(ctx);
    MH_CHECK(sink.mouseoverWrites.size() == 1);
    MH_CHECK(sink.mouseoverWrites[0] == 0xDDDD);
}

void Test_Mouseover_DisabledClearsCache() {
    std::printf("Test_Mouseover_DisabledClearsCache\n");
    MockSink sink;
    MouseoverHandler h(sink);
    TargetingDecision d{};
    d.softTarget = 0xDDDD;
    auto ctx = Ctx(/*soft=*/0, /*lastMouseover=*/0xDDDD, /*enabled=*/false);
    h.OnTickResult(ctx, d);
    MH_CHECK(ctx.lastWrittenMouseover == kNoGuid);
}

void Test_HardTarget_CommitsBracketedWrite() {
    std::printf("Test_HardTarget_CommitsBracketedWrite\n");
    MockSink sink;
    HardTargetHandler h(sink);
    TargetingDecision d{};
    d.kind = DecisionKind::SetHardTarget;
    d.hardTarget = 0xEEEE;
    d.reason = "test";
    auto ctx = Ctx(/*soft=*/0);
    h.OnTickResult(ctx, d);
    MH_CHECK(sink.beginCount == 1);
    MH_CHECK(sink.endCount == 1);
    MH_CHECK(sink.activeTargetWrites.size() == 1);
    MH_CHECK(sink.activeTargetWrites[0] == 0xEEEE);
}

void Test_HardTarget_SkipsNonSetHardTargetDecisions() {
    std::printf("Test_HardTarget_SkipsNonSetHardTargetDecisions\n");
    MockSink sink;
    HardTargetHandler h(sink);
    TargetingDecision d{};
    d.kind = DecisionKind::NoChange;
    auto ctx = Ctx(/*soft=*/0);
    h.OnTickResult(ctx, d);
    MH_CHECK(sink.activeTargetWrites.empty());
    MH_CHECK(sink.beginCount == 0);
}

void Test_HardTarget_DisabledSuppresses() {
    std::printf("Test_HardTarget_DisabledSuppresses\n");
    MockSink sink;
    HardTargetHandler h(sink);
    TargetingDecision d{};
    d.kind = DecisionKind::SetHardTarget;
    d.hardTarget = 0xEEEE;
    d.reason = "test";
    auto ctx = Ctx(/*soft=*/0, /*lastMouseover=*/0, /*enabled=*/false);
    h.OnTickResult(ctx, d);
    MH_CHECK(sink.activeTargetWrites.empty());
}

} // namespace

void RunMechanismHandlerTests() {
    Test_Action_WritesSoftPickWhenCursorIdle();
    Test_Action_YieldsWhenCursorAsserts();
    Test_Action_RepinsWhenClientClearedSlot();
    Test_Action_DiagnosticSuppresses();
    Test_Action_DisabledSuppresses();
    Test_Action_NeverWritesActiveTarget();
    Test_Mouseover_TickStoresDecision();
    Test_Mouseover_FrameWritesCachedValue();
    Test_Mouseover_DisabledClearsCache();
    Test_HardTarget_CommitsBracketedWrite();
    Test_HardTarget_SkipsNonSetHardTargetDecisions();
    Test_HardTarget_DisabledSuppresses();
}
