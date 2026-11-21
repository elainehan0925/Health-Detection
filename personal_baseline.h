// Automatically learn the initial personal blink rate and reference head direction.
#pragma once
#include "eye_activity.h"
#include <algorithm>
#include <cmath>

struct PersonalBaseline {
    double baselineSeconds = 0;
    int baselineBlinks = 0;
    double baselineRate = -1;
    bool baselineFinished = false;
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
    int lastBlinkCount = 0;

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
        int newBlinks = std::max(0, eyes.blinks-lastBlinkCount);
        lastBlinkCount = eyes.blinks;
        previousValid = valid;
        previousResting = resting;
        if (!continuous) return;

        if (!baselineFinished) {
            baselineSeconds += dt;
            baselineBlinks += newBlinks;
            if (baselineSeconds >= 60) {
                baselineFinished = true;
                if (baselineBlinks > 0) baselineRate = baselineBlinks*60.0/baselineSeconds;
            }
        }
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
