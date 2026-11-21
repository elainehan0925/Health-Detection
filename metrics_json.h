// Write the five agreed camera measurements to a complete, replaceable JSON snapshot.
#pragma once
#include "rest_policy.h"
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
                             const EyeActivity& eyes, double session, double baselineRate, const RestPolicy& rest) {
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
    jsonNumber(out, baselineRate);
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
