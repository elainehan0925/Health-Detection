// Automatically learn the initial reference head direction for away detection.
#pragma once
#include "eye_activity.h"
#include <algorithm>
#include <cmath>

struct PersonalBaseline {
    double referenceSeconds = 0;
    double pitchSum = 0;
    double yawSum = 0;
    double referencePitch = 0;
    double referenceYaw = 0;
    bool referenceReady = false;
    double positionThresholdDegrees = 30;
    double lastTime = -1;
    bool previousValid = false;
    bool previousResting = false;

    void pause() {
        lastTime = -1;
        previousValid = false;
    }

    bool isHeadAway(bool valid, double pitch, double yaw) const {
        if (!valid || !referenceReady) return false;
        return std::abs(pitch-referencePitch) > positionThresholdDegrees ||
               std::abs(yaw-referenceYaw) > positionThresholdDegrees;
    }

    void update(double now, bool valid, double pitch, double yaw,
                const EyeActivity& eyes, bool takingRest) {
        double dt = 0;
        if (lastTime >= 0) dt = std::max(0.0, now-lastTime);
        lastTime = now;
        bool longClosed = eyes.closed && eyes.currentClosed >= EyeActivity::longThreshold;
        bool resting = takingRest || longClosed;
        bool continuous = valid && previousValid && !resting && !previousResting &&
                          dt > 0 && dt <= EyeActivity::maxGap &&
                          std::isfinite(pitch) && std::isfinite(yaw);
        previousValid = valid;
        previousResting = resting;
        if (!continuous) return;

        if (!referenceReady) {
            referenceSeconds += dt;
            pitchSum += pitch*dt;
            yawSum += yaw*dt;
            if (referenceSeconds >= 10) {
                referencePitch = pitchSum/referenceSeconds;
                referenceYaw = yawSum/referenceSeconds;
                referenceReady = true;
            }
        }
    }
};
