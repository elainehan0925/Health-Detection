// Write cumulative baselines, one-minute event rates and rest measurements as a complete JSON snapshot.
#pragma once
#include "rest_policy.h"
#include "eye_fatigue.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

inline void jsonNumber(std::ostream& out, double value) {
    if (value >= 0 && std::isfinite(value)) out << value;
    else out << "null";
}

inline bool writeMetricsJson(const std::filesystem::path& path,
                             const EyeActivity& eyes, double session, const RestPolicy& rest) {
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
    out << std::fixed << std::setprecision(3);
    out << "{\n  \"blink_rate_per_minute\": ";
    jsonNumber(out, eyes.rate(session));
    out << ",\n  \"baseline_blinks_per_minute\": ";
    jsonNumber(out, eyes.baselineBlinkRate());
    out << ",\n  \"yawn_rate_per_minute\": ";
    jsonNumber(out, eyes.yawnRate(session));
    out << ",\n  \"baseline_yawns_per_minute\": ";
    jsonNumber(out, eyes.baselineYawnRate());
    out << ",\n  \"eyes_closure_total_seconds\": " << eyes.totalClosed
        << ",\n  \"yawn_count\": " << eyes.yawns
        << ",\n  \"rest_seconds_last_20_minutes\": " << rest.recordedRest(session, eyes, 1200) << "\n}\n";
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
