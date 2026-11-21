#include <cmath>
#include <fstream>
#include <iostream>
using namespace std;
#include <algorithm>
#include <chrono>
#include <limits>
#include <thread>

#include "json.hpp"
#include "posture_fatigue.h"
#include "eye_fatigue.h"


using json = nlohmann::json;

void continuous_work(bool keyboard_activity, bool mouse_activity,
                     bool person_present, double &worktime, double &resttime);
bool readData(ifstream &inFile, json &latest);
double calculate_keyboard(double err_percentage, double typingSpeedScore);
double error_percentage_keyboard(bool kDataRead, bool activity,
                                 double error_count, double count);
double typing_speed_score(bool kDataRead, bool activity, double type_time,
                          double typing_speed, double end);
double calculate_mouse(double mouseSpeedScore, double clickSpeedScore);
double mouse_speed_score(bool mDataRead, bool activity,
                         double total_moving_time, double mouse_speed);
double click_speed_score(bool mDataRead, bool activity,
                         double total_click_speed);

int main() {
  int increment = 0;
  auto next_update = chrono::steady_clock::now();
  double worktime = 0.0;
  double resttime = 0.0;
  double worktime_score = 1.0;
  double keyboardScore = 0;
  double mouseScore = 0;
  double postureScore = 0;
  double blinkScore = 0;
  double totalScore = 0;
  PostureFatigueTracker postureTracker;

  // get data every 5 seconds
  while (true) {
    if (increment == 360) {
      if (resttime >= 300) {
        worktime_score = 0.0;
      } else {
        worktime_score = 1.0;
      }
      worktime = 0.0;
      resttime = 0.0;
      increment = 0;
    }
    ifstream inFileK("keyboard.json");
    ifstream inFileM("mouse.json");
    ifstream inFileP("posture.json");
    ifstream inFileB("blink.json");

    bool kDataRead = false;
    bool mDataRead = false;
    bool pDataRead = false;
    bool bDataRead = false;

    // keyboard variables
    json latestK;
    double count;
    double error_count;
    double type_time;
    double typing_speed;
    double interval_end;
    double last_key_pressed_time;
    bool activityK;

    // read keyboard input

    if (readData(inFileK, latestK)) {
      kDataRead = true;
      count = latestK.at("Number of keys entered: ").get<double>();
      error_count = latestK.at("Number of errors made: ").get<double>();
      type_time = latestK.at("Active typing time: ").get<double>();
      typing_speed = latestK.at("Average typing speed: ").get<double>();
      interval_end = latestK.at("Interval end time: ").get<double>();
      last_key_pressed_time = latestK.at("Last key press time: ").get<double>();
      activityK = latestK.at("Activity: ").get<bool>();
    }

    // mouse variables
    json latestM;
    double moving_time;
    double total_distance;
    double average_speed;
    double left_clicks;
    double right_clicks;
    double left_click_speed;
    double right_click_speed;
    double total_click_speed;
    double last_mouse_time;
    bool activityM;

    // read mouse input
    if (readData(inFileM, latestM)) {
      mDataRead = true;
      moving_time = latestM.at("total_moving_time: ").get<double>();
      total_distance = latestM.at("total_distance: ").get<double>();
      average_speed = latestM.at("average_speed: ").get<double>();
      left_clicks = latestM.at("left_clicks: ").get<double>();
      right_clicks = latestM.at("right_clicks: ").get<double>();
      left_click_speed = latestM.at("left_click_speed: ").get<double>();
      right_click_speed = latestM.at("right_click_speed: ").get<double>();
      total_click_speed = latestM.at("total_click_speed: ").get<double>();
      last_mouse_time = latestM.at("last_activity_time: ").get<double>();
      activityM = latestM.at("activity: ").get<bool>();
    }

    // posture variables
    json latestP;
    double goodToBad;
    double goodTime;
    double badTime;
    bool reallyBadPosture;
    bool personThere;
    double time;

    // read posture input
    if (readData(inFileP, latestP)) {
      pDataRead = true;
      goodToBad = latestP.at("goodToBad: ").get<double>();
      goodTime = latestP.at("goodTime: ").get<double>();
      badTime = latestP.at("badTime: ").get<double>();
      reallyBadPosture = latestP.at("reallyBadPosture: ").get<bool>();
      personThere = latestP.at("personThere: ").get<bool>();
      time = latestP.at("time: ").get<double>();
    }

    // blink variables
    json latestB;
    double blink_rate_per_minute;
    double baseline_blinks_per_minute;
    double yawn_rate_per_minute;
    double baseline_yawns_per_minute;
    double eye_closure_total_seconds;
    double yawn_count;
    double rest_seconds_last_20_minutes;

    // read blink input
    if (readData(inFileB, latestB)) {
      bDataRead = true;
      blink_rate_per_minute =
          latestB.at("blink_rate_per_minute: ").get<double>();
      baseline_blinks_per_minute =
          latestB.at("baseline_blinks_per_minute: ").get<double>();
      yawn_rate_per_minute = latestB.at("yawn_rate_per_minute: ").get<double>();
      baseline_yawns_per_minute =
          latestB.at("baseline_yawns_per_minute: ").get<double>();
      eye_closure_total_seconds =
          latestB.at("eye_closure_total_seconds: ").get<double>();
      yawn_count = latestB.at("yawn_count: ").get<double>();
      rest_seconds_last_20_minutes =
          latestB.at("rest_seconds_last_20_minutes: ").get<double>();
    }

    if (kDataRead && mDataRead && pDataRead) {
      continuous_work(activityK, activityM, personThere, worktime, resttime);
    }
    if (kDataRead) {
      double err_percentage =
          error_percentage_keyboard(kDataRead, activityK, error_count, count);
      double typingSpeedScore = typing_speed_score(
          kDataRead, activityK, type_time, typing_speed, interval_end);
      keyboardScore = calculate_keyboard(err_percentage, typingSpeedScore);
    }
    if (mDataRead) {
      double mouseSpeedScore =
          mouse_speed_score(mDataRead, activityM, moving_time, average_speed);
      double clickSpeedScore =
          click_speed_score(mDataRead, activityM, total_click_speed);
      mouseScore = calculate_mouse(mouseSpeedScore, clickSpeedScore);
    }
    if (pDataRead) {
      PostureFatigue result =
          calculatePostureFatigue(goodTime, badTime, reallyBadPosture);

      if (result.status == "ok") {
        postureScore = result.index;
      }
    }
    blinkScore = numeric_limits<double>::quiet_NaN();
    if (bDataRead) {
      EyeFatigue result = calculateEyeFatigue(
          blink_rate_per_minute, baseline_blinks_per_minute,
          yawn_rate_per_minute, baseline_yawns_per_minute,
          rest_seconds_last_20_minutes);

      if (result.status == "ok") {
        blinkScore = result.index;
      }
    }
    totalScore = 0.2 * keyboardScore + 0.2 * mouseScore + 0.2 * postureScore +
                 0.2 * blinkScore + 0.2 * worktime_score;

    json output;
    output["systemic_fatigue_score"] = totalScore;

    ofstream outFile("metrics.json");

    if (outFile) {
      outFile << output.dump(4) << '\n';
    } else {
      cerr << "Could not write metrics.json\n";
    }

    next_update += chrono::seconds(5);
    this_thread::sleep_until(next_update);
    increment++;
  }
}

bool readData(ifstream &inFile, json &latest) {
  if (inFile.fail()) {
    cout << "data could not be accessed" << endl;
    return false;
  }
  try {
    json data;
    inFile >> data;

    if (!data.is_array() || data.empty()) {
      cout << "No data available" << endl;
      return false;
    }

    latest = data.back();
    return true;
  } catch (json::exception &error) {
    cout << "could not read json" << endl;
    return false;
  }
}

void continuous_work(bool keyboard_activity, bool mouse_activity,
                     bool person_present, double &worktime, double &resttime) {
  if ((keyboard_activity == false && mouse_activity == false) &&
      (person_present == false)) {
    resttime += 5;
  } else {
    worktime += 5;
  }
}

double calculate_keyboard(double err_percentage, double typingSpeedScore) {
  if (!isfinite(err_percentage) || !isfinite(typingSpeedScore)) {
    return numeric_limits<double>::quiet_NaN();
  }
  double normalized_typing = min(abs(typingSpeedScore) / 3.0, 1.0);
  return 0.55 * err_percentage + 0.45 * normalized_typing;
  // update avg time every 5 minutes
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
  if (kDataRead && activity) {
    // update avg_speed once received new data
    if (nth_interval >= 2) {
      std_dev = sqrt(M2 / (nth_interval - 1));
      if (std_dev > 0) {
        dev = typing_speed - avg_speed;
        z_score = dev / std_dev;  // z = (typing speed - avg speed) / std dev;
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

      std_dev = sqrt(M2 / (nth_interval));
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
  // update avg time every 5 minutes
}

double mouse_speed_score(bool mDataRead, bool activity,
                         double total_moving_time, double mouse_speed) {
  static double avg_speed = 0;
  double std_dev;
  double dev, z_score = numeric_limits<double>::quiet_NaN();
  static double M2 = 0;  // sum of squared derivations
  static int nth_interval = 0;
  if (mDataRead && activity && total_moving_time > 0) {
    // update avg_speed once received new data
    if (nth_interval >= 2) {
      std_dev = sqrt(M2 / (nth_interval - 1));
      if (std_dev > 0) {
        dev = mouse_speed - avg_speed;
        z_score = dev / std_dev;  // z = (typing speed - avg speed) / std dev;
      }
    }
    if (nth_interval == 0) {
      avg_speed = mouse_speed;
      M2 += (mouse_speed - avg_speed) * (mouse_speed - avg_speed);
    } else {
      double delta = mouse_speed - avg_speed;

      avg_speed = (nth_interval * avg_speed + mouse_speed) / (nth_interval + 1);

      M2 += delta * (mouse_speed - avg_speed);

      std_dev = sqrt(M2 / (nth_interval));
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
  if (mDataRead && activity) {
    // update avg_speed once received new data
    if (nth_interval >= 2) {
      std_dev = sqrt(M2 / (nth_interval - 1));
      if (std_dev > 0) {
        dev = total_click_speed - avg_speed;
        z_score = dev / std_dev;  // z = (typing speed - avg speed) / std dev;
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
/*
double typing_speed_score(bool activity, double type_time, double typing_speed)
{ static double avg_speed = 0; static double M2 = 0; static int count = 0;

    double variance, std_dev, dev, z_score;

    if (!activity || type_time <= 0) {
        return 0;
    }

    // Calculate Z-score using previous data
    if (count >= 2) {
        variance = M2 / (count - 1);
        std_dev = sqrt(variance);

        if (std_dev > 0) {
            dev = typing_speed - avg_speed;
            z_score = dev / std_dev;
        } else {
            z_score = 0;
        }
    } else {
        z_score = 0;
    }

    // Update mean and variance
    count++;

    double delta = typing_speed - avg_speed;
    avg_speed += delta / count;
    double delta2 = typing_speed - avg_speed;

    M2 += delta * delta2;

    return z_score;
}*/
