#include "../src/SceneWall.h"

#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char *message)
{
    if (condition)
        return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

QList<bool> eligible(std::initializer_list<bool> flags)
{
    QList<bool> result;
    for (bool flag : flags)
        result.append(flag);
    return result;
}

void testRoundRobinCoversAllVisible()
{
    // Four visible thumbnails, budget 2: every item renders within two ticks,
    // none skipped while visible.
    ThumbnailScheduler scheduler;
    QSet<int> seen;
    const QList<int> first = scheduler.pickNext(eligible({true, true, true, true}));
    check(first.size() == 2, "first tick fills budget");
    const QList<int> second = scheduler.pickNext(eligible({true, true, true, true}));
    check(second.size() == 2, "second tick fills budget");
    for (int i : first)
        seen.insert(i);
    for (int i : second)
        seen.insert(i);
    check(seen.size() == 4, "round-robin covers all visible within two ticks");
    const QList<int> third = scheduler.pickNext(eligible({true, true, true, true}));
    check(third == first, "round-robin wraps around fairly");
}

void testTickNeverExceedsBudget()
{
    ThumbnailScheduler scheduler;
    scheduler.maxReads = 2;
    for (int tick = 0; tick < 5; ++tick) {
        const QList<int> picked =
                scheduler.pickNext(eligible({true, true, true, true, true, true}));
        check(picked.size() <= 2, "one tick never exceeds maxGpuReadsPerTick");
    }
}

void testHiddenAndCollapsedSkipped()
{
    ThumbnailScheduler scheduler;
    // Index 1 collapsed, index 3 hidden: neither consumes budget.
    const QList<int> picked = scheduler.pickNext(eligible({true, false, true, false}));
    check(picked.size() == 2, "skipped widgets free budget for visible ones");
    check(!picked.contains(1) && !picked.contains(3), "hidden/collapsed never picked");
    check(picked.contains(0) && picked.contains(2), "visible widgets use freed budget");
}

void testResumeAfterShow()
{
    ThumbnailScheduler scheduler;
    scheduler.pickNext(eligible({true, false, true}));
    // Widget 1 shown again (showEvent): rejoins rotation within a full cycle.
    QSet<int> seen;
    for (int tick = 0; tick < 2; ++tick) {
        const QList<int> picked = scheduler.pickNext(eligible({true, true, true}));
        for (int i : picked)
            seen.insert(i);
    }
    check(seen.contains(1), "widget resumes after showEvent");
    check(seen.size() == 3, "all widgets covered after resume");
}

void testIntervals()
{
    check(ThumbnailScheduler::NORMAL_INTERVAL_MS == 500, "normal interval 500ms");
    check(ThumbnailScheduler::realtimeIntervalForFps(0.0) == 33, "realtime fallback 33ms");
    check(ThumbnailScheduler::realtimeIntervalForFps(-5.0) == 33, "realtime negative fps falls back");
    check(ThumbnailScheduler::realtimeIntervalForFps(60.0) == 17, "realtime 60fps gives 17ms");
    check(ThumbnailScheduler::realtimeIntervalForFps(30.0) == 33, "realtime 30fps gives 33ms");
    check(ThumbnailScheduler::DEFAULT_MAX_READS == 2, "maxGpuReadsPerTick is 2");
}
} // namespace

int main()
{
    testRoundRobinCoversAllVisible();
    testTickNeverExceedsBudget();
    testHiddenAndCollapsedSkipped();
    testResumeAfterShow();
    testIntervals();

    if (failures)
        return 1;
    std::puts("All scheduler tests passed.");
    return 0;
}
