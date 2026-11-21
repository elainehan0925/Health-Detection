// Combine rest intervals and decide whether the latest 20 minutes need a rest reminder.
#pragma once
#include "eye_activity.h"

struct RestPolicy {
    static constexpr bool testMode = false;
    double window = testMode ? 60 : 1200;
    double target = testMode ? 3 : 60;
    double snoozeSeconds = testMode ? 10 : 120;
    double minCoverage = 0.8;
    IntervalSeries declaredBreaks;
    IntervalSeries awayBreaks;
    bool manual = false;
    double manualStart = 0;
    double snoozeUntil = 0;
    double awayConfirmationSeconds = 1.0;
    bool awayCandidate = false;
    bool awayActive = false;
    double awayStart = 0;
    double lastAwayUpdate = -1;

    void toggleManual(double now) {
        if (manual) {
            declaredBreaks.add(manualStart, now);
            manual = false;
        } else {
            manual = true;
            manualStart = now;
        }
    }

    void snooze(double now) { snoozeUntil = now + snoozeSeconds; }

    void pauseAway(double now) {
        if (awayActive) awayBreaks.add(awayStart, now);
        awayCandidate = false;
        awayActive = false;
        lastAwayUpdate = -1;
    }

    void updateAway(double now, bool away) {
        if (lastAwayUpdate >= 0 && now - lastAwayUpdate > EyeActivity::maxGap) {
            if (awayActive) awayBreaks.add(awayStart, lastAwayUpdate);
            awayCandidate = false;
            awayActive = false;
        }
        lastAwayUpdate = now;
        if (away) {
            if (!awayCandidate) {
                awayCandidate = true;
                awayStart = now;
            }
            if (now - awayStart + 1e-8 >= awayConfirmationSeconds) awayActive = true;
        } else {
            if (awayActive) awayBreaks.add(awayStart, now);
            awayCandidate = false;
            awayActive = false;
        }
        awayBreaks.trim(now);
    }

    std::vector<TimeSpan> restSpans(double now, const EyeActivity& eyes) const {
        std::vector<TimeSpan> spans = eyes.restSpans();
        spans.insert(spans.end(), declaredBreaks.spans.begin(), declaredBreaks.spans.end());
        spans.insert(spans.end(), awayBreaks.spans.begin(), awayBreaks.spans.end());
        if (manual) spans.push_back({manualStart, now});
        if (awayActive) spans.push_back({awayStart, now});
        return spans;
    }

    double recordedRest(double now, const EyeActivity& eyes, double seconds = 1200) const {
        return coveredSeconds(restSpans(now, eyes), std::max(0.0, now - seconds), now);
    }

    struct View {
        double rest = 0;
        double coverage = 0;
        double missing = 0;
        double windowElapsed = 0;
        bool due = false;
        bool unknown = false;
        bool warming = true;
        bool takingBreak = false;
    };

    View view(double now, const EyeActivity& eyes, bool paused = false) {
        declaredBreaks.trim(now);
        awayBreaks.trim(now);
        View result;
        result.windowElapsed = std::min(window, now);
        result.warming = now < window;
        result.takingBreak = manual || awayActive;
        std::vector<TimeSpan> observed = eyes.validTime.spans;
        observed.insert(observed.end(), declaredBreaks.spans.begin(), declaredBreaks.spans.end());
        observed.insert(observed.end(), awayBreaks.spans.begin(), awayBreaks.spans.end());
        if (manual) observed.push_back({manualStart, now});
        if (awayActive) observed.push_back({awayStart, now});
        double begin = std::max(0.0, now - window);
        result.rest = recordedRest(now, eyes, window);
        if (result.windowElapsed > 0)
            result.coverage = coveredSeconds(observed, begin, now) / result.windowElapsed;
        result.missing = std::max(0.0, target - result.rest);
        result.unknown = !result.warming && result.coverage < minCoverage;
        result.due = !paused && !result.takingBreak && !result.warming && !result.unknown &&
                     result.missing > 1e-6 && now >= snoozeUntil;
        return result;
    }
};
