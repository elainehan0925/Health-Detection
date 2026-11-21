#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>
using namespace std;

#include "json.hpp"
#include "posture_fatigue.h"
#include "eye_fatigue.h"

using json = nlohmann::json;

void continuous_work(bool keyboard_activity, bool mouse_activity,
                     bool person_present, double elapsed,
                     double &worktime, double &resttime);
bool readData(ifstream &inFile, json &latest, const char* filename,
              filesystem::file_time_type* written_at = nullptr);
double calculate_keyboard(double err_percentage, double typingSpeedScore);
double error_percentage_keyboard(bool kDataRead, bool activity,
                                 double error_count, double count);
double typing_speed_score(bool kDataRead, bool activity, double type_time,
                          double typing_speed, double end);
double calculate_mouse(double mouseSpeedScore, double clickSpeedScore);
double mouse_speed_score(bool mDataRead, bool activity,
                         double total_moving_time, double mouse_speed);
double click_speed_score(bool mDataRead, bool activity, double total_click_speed);

int main() {
  auto next_update = chrono::steady_clock::now();
  auto last_check = next_update;
  auto window_start = next_update;
  double worktime = 0.0;
  double resttime = 0.0;
  const double unavailable = numeric_limits<double>::quiet_NaN();
  double worktime_score = unavailable;
  double keyboardScore = unavailable;
  double mouseScore = unavailable;
  double postureScore = unavailable;
  double blinkScore = unavailable;
  json previousK;
  auto previousMouseWrite = filesystem::file_time_type::min();

  // Read the latest measurements and write a score every five seconds.
  while (true) {
    ifstream inFileK("keyboard.json");
    ifstream inFileM("mouse.json");
    ifstream inFileP("posture.json");
    ifstream inFileB("blink.json");
    bool kDataRead = false, mDataRead = false, pDataRead = false;
    bool activityK = false, activityM = false, personThere = false;
    json latestK, latestM, latestP, latestB;

    if (readData(inFileK, latestK, "keyboard.json")) {
      try {
        double count = latestK.at("Number of keys entered: ").get<double>();
        double error_count = latestK.at("Number of errors made: ").get<double>();
        double type_time = latestK.at("Active typing time: ").get<double>();
        double typing_speed = latestK.at("Average typing speed: ").get<double>();
        double interval_end = latestK.at("Interval end time: ").get<double>();
        if (!isfinite(count) || !isfinite(error_count) || !isfinite(type_time) ||
            !isfinite(typing_speed) || !isfinite(interval_end) || count < 0 ||
            error_count < 0 || error_count > count || type_time < 0 || typing_speed < 0) {
          throw runtime_error("invalid keyboard measurements");
        }
        // Older keyboard records have no Activity field. A press establishes activity.
        activityK = latestK.value("Activity: ", false) || count > 0;
        kDataRead = true;
        if (previousK.is_null() || interval_end !=
            previousK.at("Interval end time: ").get<double>()) {
          if (!activityK) {
            keyboardScore = 0.0;
          } else {
            double errors = error_percentage_keyboard(true, activityK, error_count, count);
            double speed = typing_speed_score(true, activityK, type_time,
                                              typing_speed, interval_end);
            keyboardScore = calculate_keyboard(errors, speed);
          }
          previousK = latestK;
        }
      } catch (const exception &error) {
        cerr << "Keyboard data: " << error.what() << '\n';
        kDataRead = false;
      }
    }
    if (!kDataRead) keyboardScore = unavailable;

    filesystem::file_time_type mouseWritten;
    if (readData(inFileM, latestM, "mouse.json", &mouseWritten)) {
      try {
        double moving_time = latestM.at("total_moving_time: ").get<double>();
        double average_speed = latestM.at("average_speed: ").get<double>();
        double total_click_speed = latestM.at("total_click_speed: ").get<double>();
        if (!isfinite(moving_time) || !isfinite(average_speed) ||
            !isfinite(total_click_speed) || moving_time < 0 ||
            average_speed < 0 || total_click_speed < 0) {
          throw runtime_error("invalid mouse measurements");
        }
        activityM = latestM.at("activity: ").get<bool>() ||
                    moving_time > 0 || total_click_speed > 0;
        mDataRead = true;
        if (mouseWritten != previousMouseWrite) {
          if (!activityM) {
            mouseScore = 0.0;
          } else {
            // Clicking without movement, or moving without clicks, is valid activity.
            double movement = moving_time > 0
                ? mouse_speed_score(true, true, moving_time, average_speed) : 0.0;
            double clicks = total_click_speed > 0
                ? click_speed_score(true, true, total_click_speed) : 0.0;
            mouseScore = calculate_mouse(movement, clicks);
          }
          previousMouseWrite = mouseWritten;
        }
      } catch (const exception &error) {
        cerr << "Mouse data: " << error.what() << '\n';
        mDataRead = false;
      }
    }
    if (!mDataRead) mouseScore = unavailable;

    postureScore = unavailable;
    if (readData(inFileP, latestP, "posture.json")) {
      try {
        personThere = latestP.at("personThere: ").get<bool>();
        pDataRead = latestP.value("tracking_working: ", true);
        double goodTime = latestP.at("goodTime: ").get<double>();
        double badTime = latestP.at("badTime: ").get<double>();
        double reallybadTime = latestP.at("reallybadTime: ").get<double>();
        PostureFatigue result = calculatePostureFatigue(goodTime, badTime, reallybadTime);
        if (pDataRead && result.status == "ok") postureScore = result.index;
      } catch (const exception &error) {
        cerr << "Posture data: " << error.what() << '\n';
        pDataRead = false;
      }
    }

    blinkScore = unavailable;
    if (readData(inFileB, latestB, "blink.json")) {
      try {
        EyeFatigue result = calculateEyeFatigue(
            latestB.at("blink_rate_per_minute: ").get<double>(),
            latestB.at("baseline_blinks_per_minute: ").get<double>(),
            latestB.at("yawn_rate_per_minute: ").get<double>(),
            latestB.at("baseline_yawns_per_minute: ").get<double>(),
            latestB.at("rest_seconds_last_20_minutes: ").get<double>());
        if (result.status == "ok") blinkScore = result.index;
      } catch (const exception &error) {
        cerr << "Blink data: " << error.what() << '\n';
      }
    }

    auto now = chrono::steady_clock::now();
    double elapsed = chrono::duration<double>(now - last_check).count();
    last_check = now;
    bool work_observed = kDataRead && mDataRead && pDataRead && elapsed <= 15.0;
    if (work_observed) {
      continuous_work(activityK, activityM, personThere, elapsed, worktime, resttime);
    }
    double window_seconds = chrono::duration<double>(now - window_start).count();
    if (window_seconds >= 1800.0) {
      // Preserve the five-total-minutes-of-rest rule for each thirty-minute period.
      if (resttime >= 300.0) worktime_score = 0.0;
      else if (worktime + resttime >= window_seconds - 1.0) worktime_score = 1.0;
      else worktime_score = unavailable;
      worktime = 0.0;
      resttime = 0.0;
      window_start = now;
    }

    json output;
    output["components"] = json::object();
    double score_sum = 0.0;
    int available_count = 0;
    // Equal weights become 20% each when all five components are available.
    for (const auto &component : {
             pair<const char*, double>{"keyboard", keyboardScore},
             {"mouse", mouseScore}, {"posture", postureScore},
             {"blink", blinkScore}, {"work_rest", worktime_score}}) {
      if (isfinite(component.second) && component.second >= 0 && component.second <= 1) {
        output["components"][component.first] = component.second;
        score_sum += component.second;
        available_count++;
      } else {
        output["components"][component.first] = nullptr;
      }
    }
    output["systemic_fatigue_score"] = available_count > 0
        ? json(clamp(100.0 * score_sum / available_count, 0.0, 100.0)) : json(nullptr);
    output["status"] = available_count == 0 ? "insufficient_data" : "ok";
    output["available_components"] = available_count;
    output["timestamp"] = chrono::duration<double>(
        chrono::system_clock::now().time_since_epoch()).count();

    // Publish only a complete JSON document, including when readers run concurrently.
    ofstream outFile("metrics.tmp", ios::trunc);
    outFile << output.dump(4) << '\n';
    outFile.close();
    if (!outFile) {
      cerr << "Could not write metrics.tmp\n";
    } else {
#ifdef _WIN32
      if (!MoveFileExW(L"metrics.tmp", L"metrics.json", MOVEFILE_REPLACE_EXISTING))
        cerr << "Could not replace metrics.json\n";
#else
      error_code error;
      filesystem::rename("metrics.tmp", "metrics.json", error);
      if (error) cerr << "Could not replace metrics.json: " << error.message() << '\n';
#endif
    }

    next_update += chrono::seconds(5);
    if (next_update <= chrono::steady_clock::now()) {
      next_update = chrono::steady_clock::now() + chrono::seconds(5);
    }
    this_thread::sleep_until(next_update);
  }
}

bool readData(ifstream &inFile, json &latest, const char* filename,
              filesystem::file_time_type* written_at) {
  if (!inFile) return false;
  error_code error;
  auto written = filesystem::last_write_time(filename, error);
  if (error) return false;
  double age = chrono::duration<double>(filesystem::file_time_type::clock::now() - written).count();
  if (age > 15.0 || age < -5.0) return false;
  try {
    json data;
    inFile >> data;
    if (data.is_array()) {
      if (data.empty()) return false;
      latest = data.back();
    } else {
      latest = data;
    }
    if (!latest.is_object() || latest.empty()) return false;
    // Accept both Python's existing labels with ': ' and ordinary JSON field names.
    json normalized = json::object();
    for (auto it = latest.begin(); it != latest.end(); ++it) {
      string key = it.key();
      while (!key.empty() && (key.back() == ' ' || key.back() == ':')) key.pop_back();
      if (normalized.contains(key + ": ")) {
        cerr << "Ambiguous JSON fields in " << filename << '\n';
        return false;
      }
      normalized[key + ": "] = it.value();
    }
    latest = normalized;
    if (written_at) *written_at = written;
    return true;
  } catch (const json::exception &error) {
    cerr << "Could not read " << filename << ": " << error.what() << '\n';
    return false;
  }
}

void continuous_work(bool keyboard_activity, bool mouse_activity,
                     bool person_present, double elapsed,
                     double &worktime, double &resttime) {
  if (!keyboard_activity && !mouse_activity && !person_present) resttime += elapsed;
  else worktime += elapsed;
}

double calculate_keyboard(double err_percentage, double typingSpeedScore) {
  if (!isfinite(err_percentage) || !isfinite(typingSpeedScore)) {
    return numeric_limits<double>::quiet_NaN();
  }
  double normalized_typing = min(abs(typingSpeedScore) / 3.0, 1.0);
  return 0.55 * err_percentage + 0.45 * normalized_typing;
}

double error_percentage_keyboard(bool kDataRead, bool activity,
                                 double error_count, double count) {
  if (!kDataRead || !activity || count <= 0) {
    return numeric_limits<double>::quiet_NaN();
  }

  return clamp(error_count / count, 0.0, 1.0);
}

double typing_speed_score(bool kDataRead, bool activity, double type_time,
                          double typing_speed, double end) {
  static double avg_speed = 0;
  double std_dev;
  double dev, z_score = numeric_limits<double>::quiet_NaN();
  static double M2 = 0;  // sum of squared derivations
  static int nth_interval = 0;
  if (kDataRead && activity && type_time > 0 && isfinite(end) &&
      isfinite(typing_speed) && typing_speed >= 0) {
    // update avg_speed once received new data
    if (nth_interval >= 2) {
      std_dev = sqrt(max(0.0, M2) / (nth_interval - 1));
      dev = typing_speed - avg_speed;
      if (std_dev > 0) {
        z_score = dev / std_dev;
      } else {
        // A constant baseline has no spread. Cap a new deviation at three sigma.
        z_score = dev == 0 ? 0.0 : copysign(3.0, dev);
      }
    }
    if (nth_interval == 0) {
      avg_speed = typing_speed;
      M2 += (typing_speed - avg_speed) * (typing_speed - avg_speed);
    } else {
      double delta = typing_speed - avg_speed;

      avg_speed =
          (nth_interval * avg_speed + typing_speed) / (nth_interval + 1);

      M2 += delta * (typing_speed - avg_speed);

      std_dev = sqrt(max(0.0, M2) / (nth_interval));
    }
    nth_interval++;
  }
  return z_score;
}

double calculate_mouse(double mouseSpeedScore, double clickSpeedScore) {
  if (!isfinite(mouseSpeedScore) || !isfinite(clickSpeedScore)) {
    return numeric_limits<double>::quiet_NaN();
  }
  double normalized_mouse = min(abs(mouseSpeedScore) / 3.0, 1.0);
  double normalized_click = min(abs(clickSpeedScore) / 3.0, 1.0);
  return (0.6 * normalized_mouse + 0.4 * normalized_click);
}

double mouse_speed_score(bool mDataRead, bool activity,
                         double total_moving_time, double mouse_speed) {
  static double avg_speed = 0;
  double std_dev;
  double dev, z_score = numeric_limits<double>::quiet_NaN();
  static double M2 = 0;  // sum of squared derivations
  static int nth_interval = 0;
  if (mDataRead && activity && total_moving_time > 0 && isfinite(mouse_speed) && mouse_speed >= 0) {
    // update avg_speed once received new data
    if (nth_interval >= 2) {
      std_dev = sqrt(max(0.0, M2) / (nth_interval - 1));
      dev = mouse_speed - avg_speed;
      if (std_dev > 0) {
        z_score = dev / std_dev;
      } else {
        // A constant baseline has no spread. Cap a new deviation at three sigma.
        z_score = dev == 0 ? 0.0 : copysign(3.0, dev);
      }
    }
    if (nth_interval == 0) {
      avg_speed = mouse_speed;
      M2 += (mouse_speed - avg_speed) * (mouse_speed - avg_speed);
    } else {
      double delta = mouse_speed - avg_speed;

      avg_speed = (nth_interval * avg_speed + mouse_speed) / (nth_interval + 1);

      M2 += delta * (mouse_speed - avg_speed);

      std_dev = sqrt(max(0.0, M2) / (nth_interval));
    }
    nth_interval++;
  }
  return z_score;
}

double click_speed_score(bool mDataRead, bool activity,
                         double total_click_speed) {
  static double avg_speed = 0;
  double std_dev;
  double dev, z_score = numeric_limits<double>::quiet_NaN();
  static double M2 = 0;  // sum of squared derivations
  static int nth_interval = 0;
  if (mDataRead && activity && isfinite(total_click_speed) && total_click_speed >= 0) {
    // update avg_speed once received new data
    if (nth_interval >= 2) {
      std_dev = sqrt(max(0.0, M2) / (nth_interval - 1));
      dev = total_click_speed - avg_speed;
      if (std_dev > 0) {
        z_score = dev / std_dev;
      } else {
        // A constant baseline has no spread. Cap a new deviation at three sigma.
        z_score = dev == 0 ? 0.0 : copysign(3.0, dev);
      }
    }
    if (nth_interval == 0) {
      avg_speed = total_click_speed;
      M2 += (total_click_speed - avg_speed) * (total_click_speed - avg_speed);
    } else {
      double delta = total_click_speed - avg_speed;

      avg_speed =
          (nth_interval * avg_speed + total_click_speed) / (nth_interval + 1);

      M2 += delta * (total_click_speed - avg_speed);
    }
    nth_interval++;
  }
  return z_score;
}
