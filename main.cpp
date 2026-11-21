#include <iostream>
#include <fstream>
#include <cmath>
using namespace std;
#include <thread>
#include <chrono>
#include <limits>
#include <algorithm>

double calculate_weight();
double continuous_work();
double read_json();

int main(){

}

double continuous_work(){
    //a while that did not move mouse, or by keyboard
}

double calculate_keyboard(double err_percentage, double typingSpeedScore){
    if(!isfinite(err_percentage) || !isfinite(typingSpeedScore)) {
        return numeric_limits<double>::quiet_NaN();
    }
    double normalized_typing = min(abs(typingSpeedScore) / 3.0, 1.0);
    return 0.55 * err_percentage + 0.45 * normalized_typing;
    //update avg time every 5 minutes
}

double error_percentage_keyboard(bool kDataRead, bool activity, double error_count, double count){
    if (!kDataRead || !activity || count <= 0) {
        return numeric_limits<double>::quiet_NaN();
    }

    return clamp(error_count / count, 0.0, 1.0);
}

double typing_speed_score(bool kDataRead, bool activity, double type_time, double typing_speed, double end){
    static double avg_speed = 0;
    double std_dev;
    double dev, z_score = numeric_limits<double>::quiet_NaN();
    static double M2 = 0; // sum of squared derivations
    static int nth_interval = 0;
        if(kDataRead && activity){
            //update avg_speed once received new data
            if (nth_interval >= 2) {
                std_dev = sqrt(M2 / (nth_interval - 1));
                if (std_dev > 0) {
                    dev = typing_speed - avg_speed;
                    z_score = dev / std_dev; // z = (typing speed - avg speed) / std dev;
                }
            }
            if(nth_interval == 0){
                avg_speed = typing_speed;
                M2 += (typing_speed - avg_speed) * (typing_speed - avg_speed);
            }else{
                double delta = typing_speed - avg_speed;

                avg_speed = (nth_interval * avg_speed + typing_speed) / (nth_interval + 1);

                M2 += delta * (typing_speed - avg_speed);
                
                std_dev = sqrt(M2 / (nth_interval));
            }
            nth_interval++;
        }
    return z_score;
}

double calculate_mouse(double mouseSpeedScore, double clickSpeedScore){
    if(!isfinite(mouseSpeedScore) || !isfinite(clickSpeedScore)) {
        return numeric_limits<double>::quiet_NaN();
    }
    double normalized_mouse = min(abs(mouseSpeedScore) / 3.0, 1.0);
    double normalized_click = min(abs(clickSpeedScore) / 3.0, 1.0);
    return (0.6 * normalized_mouse + 0.4 * normalized_click);
    //update avg time every 5 minutes
}

double mouse_speed_score(bool mDataRead, bool activity, double total_moving_time, double mouse_speed){
    static double avg_speed = 0;
    double std_dev;
    double dev, z_score = numeric_limits<double>::quiet_NaN();
    static double M2 = 0; // sum of squared derivations
    static int nth_interval = 0;
        if(mDataRead && activity && total_moving_time > 0){
            //update avg_speed once received new data
            if (nth_interval >= 2) {
                std_dev = sqrt(M2 / (nth_interval - 1));
                if (std_dev > 0) {
                    dev = mouse_speed - avg_speed;
                    z_score = dev / std_dev; // z = (typing speed - avg speed) / std dev;
                }
            }
            if(nth_interval == 0){
                avg_speed = mouse_speed;
                M2 += (mouse_speed - avg_speed) * (mouse_speed - avg_speed);
            }else{
                double delta = mouse_speed - avg_speed;

                avg_speed = (nth_interval * avg_speed + mouse_speed) / (nth_interval + 1);

                M2 += delta * (mouse_speed - avg_speed);
                
                std_dev = sqrt(M2 / (nth_interval));
            }
            nth_interval++;
        }
    return z_score;
}

double click_speed_score(bool mDataRead, bool activity, double total_click_speed){
    static double avg_speed = 0;
    double std_dev;
    double dev, z_score = numeric_limits<double>::quiet_NaN();
    static double M2 = 0; // sum of squared derivations
    static int nth_interval = 0;
        if(mDataRead && activity){
            //update avg_speed once received new data
            if (nth_interval >= 2) {
                std_dev = sqrt(M2 / (nth_interval - 1));
                if (std_dev > 0) {
                    dev = total_click_speed - avg_speed;
                    z_score = dev / std_dev; // z = (typing speed - avg speed) / std dev;
                }
            }
            if(nth_interval == 0){
                avg_speed = total_click_speed;
                M2 += (total_click_speed - avg_speed) * (total_click_speed - avg_speed);
            }else{
                double delta = total_click_speed - avg_speed;

                avg_speed = (nth_interval * avg_speed + total_click_speed) / (nth_interval + 1);

                M2 += delta * (total_click_speed - avg_speed);
            }
            nth_interval++;
        }
    return z_score;
}
/*
double typing_speed_score(bool activity, double type_time, double typing_speed) {
    static double avg_speed = 0;
    static double M2 = 0;
    static int count = 0;

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