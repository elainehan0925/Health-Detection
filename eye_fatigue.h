// Calculate a 0-to-1 prototype fatigue index and save a separate JSON score snapshot.
#pragma once
#include "metrics_json.h"

struct EyeFatigue {
    static constexpr double blinkWeight = 0.7;
    static constexpr double yawnWeight = 0.3;
    static constexpr double maxRestCredit = 0.2;
    static constexpr double restGoalSeconds = 60.0;

    double index = -1;
    double blinkComponent = -1;
    double yawnComponent = -1;
    double restCredit = -1;
    std::string status = "insufficient_data";
};

inline EyeFatigue calculateEyeFatigue(double blink, double blinkBaseline,
                                    double yawn, double yawnBaseline,
                                    double restSeconds) {
    EyeFatigue score;
    for (double value : {blink, blinkBaseline, yawn, yawnBaseline, restSeconds}) {
        if (!std::isfinite(value) || value < 0) return score;
    }
    if (blinkBaseline == 0 || restSeconds > 1200) return score;

    score.blinkComponent = std::max(0.0, blink - blinkBaseline) / (blink + blinkBaseline);
    score.yawnComponent = std::max(0.0, yawn - yawnBaseline) / (yawn + yawnBaseline + 1.0);
    score.restCredit = EyeFatigue::maxRestCredit *
        std::clamp(restSeconds / EyeFatigue::restGoalSeconds, 0.0, 1.0);
    score.index = std::clamp(EyeFatigue::blinkWeight * score.blinkComponent +
                             EyeFatigue::yawnWeight * score.yawnComponent - score.restCredit,
                             0.0, 1.0);
    score.status = "ok";
    return score;
}

inline bool writeEyeFatigueJson(const std::filesystem::path& path,
                                const EyeActivity& eyes, double session, const RestPolicy& rest) {
    EyeFatigue score = calculateEyeFatigue(eyes.rate(session), eyes.baselineBlinkRate(),
                                         eyes.yawnRate(session), eyes.baselineYawnRate(),
                                         rest.recordedRest(session, eyes, 1200));
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return false;
    }
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::ofstream out(temporary, std::ios::trunc);
    if (!out) return false;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(6);
    out << "{\n  \"fatigue_index\": ";
    jsonNumber(out, score.index);
    out << ",\n  \"blink_component\": ";
    jsonNumber(out, score.blinkComponent);
    out << ",\n  \"yawn_component\": ";
    jsonNumber(out, score.yawnComponent);
    out << ",\n  \"rest_credit\": ";
    jsonNumber(out, score.restCredit);
    out << ",\n  \"status\": \"" << score.status << "\"\n}\n";
    out.close();
    if (!out) return false;
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(), path.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temporary, path, error);
    return !error;
#endif
}
