// Calculate a 0-to-1 posture exposure index from three mutually exclusive stage durations.
#pragma once
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <ostream>
#include <string>

struct PostureFatigue {
    static constexpr double badWeight = 0.5;
    static constexpr double reallyBadWeight = 1.0;
    double index = -1;
    double classifiedSeconds = 0;
    double intervalSeconds = 0;
    double notInFrameSeconds = 0;
    double coverage = 0;
    std::string status = "insufficient_data";
};

inline PostureFatigue calculatePostureFatigue(double goodTime, double badTime,
                                             double reallybadTime) {
    PostureFatigue score;
    if (!std::isfinite(goodTime) || !std::isfinite(badTime) ||
        !std::isfinite(reallybadTime) || goodTime < 0 || badTime < 0 || reallybadTime < 0) {
        score.status = "invalid_data";
        return score;
    }
    double total = goodTime + badTime + reallybadTime;
    if (!std::isfinite(total)) {
        score.status = "invalid_data";
        return score;
    }
    if (total == 0) return score;
    score.classifiedSeconds = total;
    score.index = std::clamp((PostureFatigue::badWeight * badTime +
                              PostureFatigue::reallyBadWeight * reallybadTime) / total,
                             0.0, 1.0);
    score.status = "ok";
    return score;
}

struct CumulativePostureSample {
    double goodTime = 0;
    double badTime = 0;
    double reallybadTime = 0;
    double time = 0;
};

struct PostureFatigueTracker {
    bool ready = false;
    CumulativePostureSample previous;

    PostureFatigue update(const CumulativePostureSample& current) {
        PostureFatigue score;
        PostureFatigue check = calculatePostureFatigue(current.goodTime, current.badTime,
                                                       current.reallybadTime);
        if (check.status == "invalid_data" || !std::isfinite(current.time) || current.time < 0 ||
            check.classifiedSeconds > current.time + 0.001) {
            score.status = "invalid_data";
            return score;
        }
        if (!ready) {
            previous = current;
            ready = true;
            score.status = "warming_up";
            return score;
        }
        if (current.time < previous.time || current.goodTime < previous.goodTime - 0.001 ||
            current.badTime < previous.badTime - 0.001 ||
            current.reallybadTime < previous.reallybadTime - 0.001) {
            previous = current;
            score.status = "baseline_reset";
            return score;
        }
        if (current.time == previous.time) {
            bool same = current.goodTime == previous.goodTime && current.badTime == previous.badTime &&
                        current.reallybadTime == previous.reallybadTime;
            score.status = same ? "duplicate_sample" : "invalid_data";
            return score;
        }
        double elapsed = current.time - previous.time;
        score = calculatePostureFatigue(std::max(0.0, current.goodTime - previous.goodTime),
                                        std::max(0.0, current.badTime - previous.badTime),
                                        std::max(0.0, current.reallybadTime - previous.reallybadTime));
        previous = current;
        score.intervalSeconds = elapsed;
        if (score.classifiedSeconds > elapsed + 0.001) {
            score.index = -1;
            score.status = "invalid_data";
            return score;
        }
        score.coverage = std::clamp(score.classifiedSeconds / elapsed, 0.0, 1.0);
        score.notInFrameSeconds = std::max(0.0, elapsed - score.classifiedSeconds);
        if (score.classifiedSeconds == 0) score.status = "not_in_frame";
        return score;
    }
};

inline bool writePostureFatigueJson(std::ostream& out, const PostureFatigue& score) {
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(6);
    out << "{\n  \"posture_fatigue_index\": ";
    if (score.status == "ok" && std::isfinite(score.index) && score.index >= 0 && score.index <= 1)
        out << score.index;
    else out << "null";
    out << ",\n  \"classified_seconds\": " << score.classifiedSeconds
        << ",\n  \"interval_seconds\": " << score.intervalSeconds
        << ",\n  \"not_in_frame_seconds\": " << score.notInFrameSeconds
        << ",\n  \"coverage\": " << score.coverage
        << ",\n  \"status\": \"" << score.status << "\"\n}\n";
    return static_cast<bool>(out);
}
