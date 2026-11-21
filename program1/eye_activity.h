// Track blinks, eye-closure duration and consecutive yawn candidates across frames.
#pragma once
#include "time_window.h"
#include <cmath>
#include <list>
#include <string>

struct EyeActivity {

    static constexpr double maxGap = 0.25;
    static constexpr double grace = 0.35;
    static constexpr double longThreshold = 1.0;
    int blinks = 0;
    int partialBlinks = 0;
    int yawns = 0;
    double totalValidSeconds = 0;
    double totalClosed = 0;
    double longClosed = 0;
    double currentClosed = 0;
    double lastTime = -1;
    double lastGood = -1;
    bool previousValid = false;
    bool previousClosed = false;
    bool closed = false;
    bool armed = false;
    bool candidate = false;
    bool eligible = false;
    bool sawClosed = false;
    bool wasLong = false;
    double candidateTime = 0;
    bool yawnActive = false;
    bool yawnArmed = false;
    double yawnTime = 0;
    double yawnUntil = -1;

    std::list<double> blinkEvents;
    std::list<double> yawnEvents;
    IntervalSeries validTime;
    IntervalSeries longClosedTime;
    IntervalSeries currentIntervals;
    std::string state = "WAITING FOR FACE";
    std::string event = "Ready - no calibration required";

    void finishClosed(){
        if(!closed)return;
        if(currentClosed>=longThreshold){
            longClosed+=currentClosed;wasLong=true;
            for (size_t i = 0; i < currentIntervals.spans.size(); ++i) {
                TimeSpan span = currentIntervals.spans[i];
                longClosedTime.add(span.begin, span.end);
            }
        }
        closed=false;currentClosed=0;currentIntervals.spans.clear();
    }
    void finishCandidate(bool reopened,double now){
        if(!candidate)return;
        if(reopened&&eligible&&candidateTime>0&&candidateTime<1.0&&!wasLong){
            ++blinks;if(!sawClosed)++partialBlinks;
            blinkEvents.push_back(now);
            event=sawClosed?"Blink counted":"Partial blink candidate counted";
        }
        candidate=false;eligible=false;candidateTime=0;sawClosed=false;wasLong=false;
    }
    void pause(double now){
        finishClosed();finishCandidate(false,now);
        previousValid=false;previousClosed=false;armed=false;lastGood=-1;lastTime=now;
        yawnActive=false;yawnArmed=false;yawnTime=0;state="PAUSED";
    }
    std::vector<TimeSpan> restSpans()const{
        std::vector<TimeSpan> result = longClosedTime.spans;
        if(closed&&currentClosed>=longThreshold)
            result.insert(result.end(),currentIntervals.spans.begin(),currentIntervals.spans.end());
        return result;
    }
    double sessionLongClosed() const {
        if (closed && currentClosed >= longThreshold) return longClosed + currentClosed;
        return longClosed;
    }

    double shortRate(double now, const std::list<double>& events) const {
        double valid = validTime.seconds(now, 60);
        double elapsed = std::min(60.0, now);
        if (valid < 10 || elapsed <= 0 || valid / elapsed < 0.7) return -1;

        int count = 0;
        for (std::list<double>::const_iterator it = events.begin(); it != events.end(); ++it) {
            if (*it > now-60 && *it <= now) ++count;
        }
        return count*60.0/valid;
    }

    double rate(double now) const {
        return shortRate(now, blinkEvents);
    }

    double yawnRate(double now) const {
        return shortRate(now, yawnEvents);
    }

    double cumulativeRate(int count) const {
        if (totalValidSeconds < 10) return -1;
        return count*60.0/totalValidSeconds;
    }

    double baselineBlinkRate() const {
        return cumulativeRate(blinks);
    }

    double baselineYawnRate() const {
        return cumulativeRate(yawns);
    }

    static constexpr double yawnOpenScore = 0.55;
    static constexpr double yawnCloseScore = 0.25;
    static constexpr double yawnMinSeconds = 1.0;
    static constexpr double yawnMaxSeconds = 10.0;

    void updateYawn(double now, bool valid, bool continuous, double dt, double jaw) {
        if (!valid || !continuous) {
            yawnActive = false;
            yawnArmed = false;
            yawnTime = 0;
            if (valid && jaw <= yawnCloseScore) yawnArmed = true;
            return;
        }

        if (yawnActive && jaw > yawnCloseScore) yawnTime += dt;
        if (jaw <= yawnCloseScore) {
            if (yawnActive && yawnTime + 1e-8 >= yawnMinSeconds &&
                yawnTime <= yawnMaxSeconds) {
                ++yawns;
                yawnEvents.push_back(now);
                yawnUntil = now + 3;
                event = "Yawn candidate - z Z Z";
            }
            yawnActive = false;
            yawnTime = 0;
            yawnArmed = true;
        } else if (jaw >= yawnOpenScore && yawnArmed && !yawnActive) {
            yawnActive = true;
            yawnArmed = false;
            yawnTime = 0;
        }
        if (yawnTime > yawnMaxSeconds) {
            yawnActive = false;
            yawnArmed = false;
            yawnTime = 0;
        }
    }

    void update(double now,bool valid,double leftBlink,double rightBlink,double jaw){
        double before = lastTime;
        double dt = 0;
        if (before >= 0) dt = now - before;
        lastTime = now;

        bool continuous = previousValid && dt > 0 && dt <= maxGap;
        updateYawn(now, valid, continuous, dt, jaw);
        if(!valid){
            eligible=false;armed=false;yawnActive=false;yawnTime=0;
            if(lastGood>=0&&now-lastGood>grace){finishClosed();finishCandidate(false,now);}
            previousValid=false;previousClosed=false;state="UNKNOWN - not rest";
            validTime.trim(now);longClosedTime.trim(now);
            return;
        }
        if(lastGood>=0&&now-lastGood>grace){finishClosed();finishCandidate(false,now);armed=false;}
        if(!continuous)eligible=false;
        if(continuous) {
            totalValidSeconds += dt;
            validTime.add(before,now);
        }
        lastGood=now;
        double mean = (leftBlink + rightBlink) * 0.5;

        bool open=mean<=0.30&&std::max(leftBlink,rightBlink)<=0.45;
        bool strong=mean>=0.55&&std::min(leftBlink,rightBlink)>=0.35;
        bool partial=mean>=0.35&&std::min(leftBlink,rightBlink)>=0.20;
        bool nextClosed=strong||(closed&&!open);
        if(candidate&&continuous)candidateTime+=dt;
        if(!candidate&&(strong||(partial&&armed&&continuous))){
            candidate=true;eligible=armed&&continuous;candidateTime=0;
            sawClosed=strong;wasLong=false;
        }

        double a=now,b=now;
        if(continuous){
            double middle=(before+now)*0.5;
            if(previousClosed&&nextClosed){a=before;b=now;}
            else if(!previousClosed&&nextClosed){a=middle;b=now;}
            else if(previousClosed&&!nextClosed){a=before;b=middle;}
        }
        double contribution=b-a;
        totalClosed+=contribution;
        if(nextClosed&&!closed){closed=true;currentClosed=0;currentIntervals.spans.clear();}
        if(closed){currentClosed+=contribution;currentIntervals.add(a,b);}
        if(candidate){sawClosed=sawClosed||nextClosed;}
        if(closed&&currentClosed>=longThreshold)wasLong=true;
        if(!nextClosed)finishClosed();
        if(open){
            finishCandidate(true,now);armed=true;state="OPEN";
        }else{
            if(candidate&&!closed&&candidateTime>=1.0){finishCandidate(false,now);armed=false;}
            if (closed) {
                if (currentClosed >= longThreshold) state = "LONG CLOSED";
                else state = "CLOSED";
            } else if (candidate) {
                state = "PARTIAL BLINK CANDIDATE";
            } else {
                state = "PARTIAL / UNKNOWN";
            }
        }

        previousClosed=nextClosed;previousValid=true;
        while(!blinkEvents.empty()&&blinkEvents.front()<=now-60)blinkEvents.pop_front();
        while(!yawnEvents.empty()&&yawnEvents.front()<=now-60)yawnEvents.pop_front();
        validTime.trim(now);longClosedTime.trim(now);
    }
};
