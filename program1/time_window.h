// Clip and merge time intervals to calculate duration without counting overlaps twice.
#pragma once
#include <algorithm>
#include <vector>

struct TimeSpan {
    double begin;
    double end;
};

inline bool earlierStart(const TimeSpan& first, const TimeSpan& second) {
    return first.begin < second.begin;
}

inline double coveredSeconds(std::vector<TimeSpan> spans, double begin, double end) {
    std::vector<TimeSpan> clipped;
    for (size_t i = 0; i < spans.size(); ++i) {
        TimeSpan span = spans[i];
        span.begin = std::max(span.begin, begin);
        span.end = std::min(span.end, end);
        if (span.end > span.begin) clipped.push_back(span);
    }
    std::sort(clipped.begin(), clipped.end(), earlierStart);

    double total = 0;
    double mergedBegin = 0;
    double mergedEnd = 0;
    bool started = false;
    for (size_t i = 0; i < clipped.size(); ++i) {
        TimeSpan span = clipped[i];
        if (!started) {
            mergedBegin = span.begin;
            mergedEnd = span.end;
            started = true;
        } else if (span.begin <= mergedEnd) {
            mergedEnd = std::max(mergedEnd, span.end);
        } else {
            total += mergedEnd - mergedBegin;
            mergedBegin = span.begin;
            mergedEnd = span.end;
        }
    }
    if (started) total += mergedEnd - mergedBegin;
    return total;
}

struct IntervalSeries {
    std::vector<TimeSpan> spans;

    void add(double begin, double end) {
        if (end <= begin) return;
        if (!spans.empty() && begin <= spans.back().end + 1e-8 && end >= spans.back().begin) {
            spans.back().begin = std::min(begin, spans.back().begin);
            spans.back().end = std::max(end, spans.back().end);
        } else {
            spans.push_back({begin, end});
        }
    }

    void trim(double now) {
        double start = now - 1200;
        std::vector<TimeSpan> kept;
        for (size_t i = 0; i < spans.size(); ++i) {
            TimeSpan span = spans[i];
            if (span.end > start) {
                span.begin = std::max(start, span.begin);
                kept.push_back(span);
            }
        }
        spans = kept;
    }

    double seconds(double now, double window) const {
        return coveredSeconds(spans, std::max(0.0, now - window), now);
    }
};
