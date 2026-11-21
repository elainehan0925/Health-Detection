// Load the detection, statistics, baseline, rest and JSON modules.
#include "eye_detector.h"
#include "rest_policy.h"
#include "personal_baseline.h"
#include "metrics_json.h"
#include "rest_reminder.h"
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <sstream>

// A signal handler only sets a flag; cleanup and JSON writing stay in the main loop.
volatile std::sig_atomic_t stopRequested = 0;

void requestStop(int) {
    stopRequested = 1;
}

// Use a monotonic clock so system clock changes do not affect elapsed time.
double nowSeconds(){
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string duration(double seconds){
    int t=std::max(0,static_cast<int>(seconds));
    return cv::format("%02d:%02d",t/60,t%60);
}

std::string decimal(double value,int precision=1){
    std::ostringstream out;out<<std::fixed<<std::setprecision(precision)<<value;return out.str();
}
// Draw a text label on the OpenCV image.
void label(cv::Mat& image,const std::string& value,int x,int y,double size=0.55,cv::Scalar color={225,230,240}){
    cv::putText(image,value,{x,y},cv::FONT_HERSHEY_SIMPLEX,size,color,1,cv::LINE_AA);
}


void drawRow(cv::Mat& canvas, const std::string& value, int x, int& y,
             cv::Scalar color = cv::Scalar(225,230,240)) {
    label(canvas, value, x, y, 0.53, color);
    y += 29;
}

// Display a silent cartoon briefly after a yawn candidate is counted.
void sleepyFace(cv::Mat& canvas){
    cv::circle(canvas,{560,130},45,{80,195,245},-1,cv::LINE_AA);
    cv::line(canvas,{535,122},{550,122},{50,60,80},3,cv::LINE_AA);
    cv::line(canvas,{568,122},{583,122},{50,60,80},3,cv::LINE_AA);
    cv::ellipse(canvas,{560,145},{8,10},0,0,360,{50,60,80},-1,cv::LINE_AA);
    label(canvas,"z Z Z",510,75,0.85,{190,130,255});
}

int main(int argc,char** argv){
    try{
        // Read optional camera, output path and diagnostic-mode arguments.
        int checkFrames=0,cameraIndex=0;
        bool preview = false;
        std::signal(SIGINT, requestStop);
        std::signal(SIGTERM, requestStop);
#ifdef SIGBREAK
        std::signal(SIGBREAK, requestStop);
#endif
        std::filesystem::path jsonPath = std::filesystem::path(__FILE__).parent_path() / "data" / "blink_metrics.json";
        for(int i=1;i<argc;++i){
            std::string arg=argv[i];
            if(arg=="--preview")preview=true;
            else if(arg=="--check")checkFrames=1;
            else if(arg=="--check-frames"&&i+1<argc)checkFrames=std::clamp(std::stoi(argv[++i]),1,300);
            else if(arg=="--camera"&&i+1<argc)cameraIndex=std::stoi(argv[++i]);
            else if(arg=="--json"&&i+1<argc)jsonPath=argv[++i];
            else{std::cerr<<"Usage: camera_preview.exe [--preview] [--camera 0] [--check] [--check-frames N] [--json path]\n";return 2;}
        }
        // Load the three vision models and open the selected camera.
        cv::setNumThreads(2);
        EyeDetector detector(std::filesystem::path(__FILE__).parent_path()/"models");
        cv::VideoCapture camera(cameraIndex,cv::CAP_DSHOW);
        if(!camera.isOpened()){std::cerr<<"Cannot open camera. Close other camera apps/check permission.\n";return 1;}
        // Request a 640x480 camera stream at 30 FPS; actual FPS may differ.
        camera.set(cv::CAP_PROP_FRAME_WIDTH,640);camera.set(cv::CAP_PROP_FRAME_HEIGHT,480);
        camera.set(cv::CAP_PROP_FPS,30);
        // Keep separate objects for event statistics, personal references and rest records.
        EyeActivity eyes;
        PersonalBaseline baseline;
        RestPolicy rest;
        RestReminder reminder;

        // Initialize display controls, real-time counters and diagnostic statistics.
        bool paused=false,details=false;
        double session=0,lastReal=nowSeconds(),fpsStart=lastReal,fps=0,lastLog=-5;
        int frames=0,checked=0,validChecked=0;
        double minimumBlink=1,maximumBlink=0;
        // Normal mode has no camera window; preview is an explicit diagnostic option.
        bool showPreview = preview && checkFrames == 0;
        if(showPreview)cv::namedWindow("Eye Monitor",cv::WINDOW_AUTOSIZE);
        std::cout<<"Camera connected. Automatic detection: no C/V calibration required.\n"<<std::flush;
        if (!checkFrames) {
            std::cout << "JSON: " << jsonPath.string() << '\n';
            std::cout << (showPreview ? "Preview mode: Q / ESC quits.\n" :
                                       "Background mode: no camera window. Press Ctrl+C to stop.\n");
        }
        double lastJson = -1;
        bool jsonFailed = false;
        cv::Mat frame;
        // Process one camera frame at a time until reading fails or the user quits.
        while(!stopRequested&&camera.read(frame)&&!frame.empty()){
            if(frame.size()!=cv::Size(640,480))cv::resize(frame,frame,{640,480});

            EyeResult eye=detector.detect(frame);

            double real=nowSeconds(),delta=std::max(0.0,real-lastReal);lastReal=real;
            ++frames;
            if(real-fpsStart>=1){fps=frames/(real-fpsStart);frames=0;fpsStart=real;}
            if(checkFrames){
                ++checked;
                if(eye.valid){
                    ++validChecked;double b=(eye.blinkLeft+eye.blinkRight)*0.5;
                    minimumBlink=std::min(minimumBlink,b);maximumBlink=std::max(maximumBlink,b);
                    if(validChecked<=3)std::cout<<"VALID blink="<<eye.blinkLeft<<'/'<<eye.blinkRight
                        <<" jaw="<<eye.jawOpen<<" pitch="<<eye.pitch<<'\n';
                }
                if(checked>=checkFrames){
                    std::cout<<"CHECK frames="<<checked<<" valid="<<validChecked;
                    if(validChecked)std::cout<<" blink range="<<minimumBlink<<".."<<maximumBlink;
                    std::cout<<'\n';return 0;
                }
                continue;
            }
            // Keep real elapsed time moving during pause so old rest records still expire.
            session += delta;
            // Update statistics only while monitoring is active; pausing does not create rest.
            if(!paused){
                int previousBlink=eyes.blinks,previousYawn=eyes.yawns;
                eyes.update(session,eye.valid,eye.blinkLeft,eye.blinkRight,eye.jawOpen);
                // Combine missing eyes, estimated gaze offset and relative head direction as away evidence.
                bool headAway = baseline.isHeadAway(eye.valid, eye.pitch, eye.yaw);
                rest.updateAway(session, !eye.valid || eye.lookingAway || headAway);
                // Learn personal references only from continuous, valid, non-rest observations.
                baseline.update(session, eye.valid, eye.pitch, eye.yaw, eyes,
                                rest.manual || rest.awayActive);
                // Print a message only when a blink or yawn count changes.
                if(showPreview&&(eyes.blinks!=previousBlink||eyes.yawns!=previousYawn))
                    std::cout<<eyes.event<<" | blinks="<<eyes.blinks<<" partial="<<eyes.partialBlinks
                             <<" yawns="<<eyes.yawns<<'\n'<<std::flush;
            }
            RestPolicy::View policy=rest.view(session,eyes,paused);
            if (!reminder.update(session, policy))
                std::cerr << "Windows rest notification could not be displayed.\n";
            // Skip all drawing, window polling and interactive controls in normal background mode.
            if (showPreview) {
                // Calculate the reminder state and the latest one-minute blink frequency.

                double rate=eyes.rate(session);

                // Build the dashboard by placing the camera image on a larger background.
                cv::Mat canvas(790,1040,CV_8UC3,cv::Scalar(24,27,35));
                frame.copyTo(canvas(cv::Rect(0,50,640,480)));
                label(canvas,"EYE MONITOR",16,30,0.75,{100,220,180});
                label(canvas,"Automatic detection - no calibration",650,30,0.53,{100,220,180});
                // Mark the tracked eye points and display the silent yawn animation when active.
                for(auto p:eye.overlay)cv::circle(canvas,{cvRound(p.x),cvRound(p.y)+50},2,{70,240,140},-1);
                if(session<eyes.yawnUntil)sleepyFace(canvas);
                // Display live statistics; these extra dashboard values are not exported to JSON.
                int x=657,y=65;
                drawRow(canvas,"Blinks: "+std::to_string(eyes.blinks)+"  (partial: "+std::to_string(eyes.partialBlinks)+")",x,y);
                drawRow(canvas,"Estimated rate: "+(rate >= 0 ? decimal(rate)+"/min" : "warming up / low coverage"),x,y);
                drawRow(canvas,"Closed now: "+decimal(eyes.currentClosed,2)+" s",x,y);
                drawRow(canvas,"Closed total: "+decimal(eyes.totalClosed,2)+" s",x,y);
                drawRow(canvas,"Long closed: "+decimal(eyes.sessionLongClosed(),2)+" s",x,y);
                drawRow(canvas,"Yawn candidates: "+std::to_string(eyes.yawns),x,y);
                drawRow(canvas,"Jaw: "+decimal(eye.jawOpen,2)+"  Open: "+decimal(eyes.yawnTime,1)+" s",x,y);
                drawRow(canvas,"Rest / "+duration(rest.window)+": "+duration(policy.rest)+" / "+duration(rest.target),x,y);
                drawRow(canvas,"Rest / last 20 min: "+decimal(rest.recordedRest(session,eyes),1)+" s",x,y);
                drawRow(canvas,"Rest data coverage: "+decimal(policy.coverage*100,0)+"%",x,y);
                drawRow(canvas,"Elapsed: "+duration(session)+(paused?"  PAUSED":""),x,y);
                drawRow(canvas,"Eye state: "+(rest.awayActive ? std::string("AWAY / REST") : eyes.state),x,y);
                drawRow(canvas,"Closed score L/R: "+decimal(eye.blinkLeft,2)+" / "+decimal(eye.blinkRight,2),x,y);
                drawRow(canvas,"FPS: "+decimal(fps),x,y);
                if(details)drawRow(canvas,eye.status,x,y,{170,180,190});
                // Show the learned blink reference or progress toward 60 valid observation seconds.
                drawRow(canvas,"Baseline blink: "+(baseline.baselineRate>0 ? decimal(baseline.baselineRate)+"/min" :
                    (baseline.baselineFinished ? std::string("unavailable") : "learning "+decimal(baseline.baselineSeconds,0)+"/60 s")),x,y);
                // Choose a status banner for pause, active rest, warm-up, insufficient data or a reminder.
                std::string message,sub;
                cv::Scalar banner(45,60,65);
                if(paused){message="Monitoring paused";sub="Paused time is not automatically credited as rest.";}
                else if(rest.manual){
                    message="User-declared break: "+duration(session-rest.manualStart);
                    sub="Press SPACE when you finish. Overlapping closed time is counted only once.";
                }else if(rest.awayActive){
                    message="Looking away: rest is being recorded.";
                    sub="Rest in latest 20 minutes: "+decimal(rest.recordedRest(session,eyes),1)+" seconds.";
                }else if(policy.warming){
                    message="Monitoring first window: "+duration(session)+" / "+duration(rest.window);
                    sub=RestPolicy::testMode ? "TEST MODE: 60-second window / 3-second rest goal." : "20-minute window / 60-second rest goal (product setting).";
                }else if(policy.unknown){
                    message="Rest record incomplete: eye tracking coverage is low.";
                    sub="Away-looking periods count after one-second confirmation.";
                }else if(policy.due){
                    banner={35,70,110};
                    message="Consider a break: recorded rest is below your "+decimal(rest.target,0)+"-second goal.";
                    sub="Recorded goal gap: "+std::to_string(static_cast<int>(std::ceil(policy.missing)))+" s. SPACE: start break; S: snooze.";
                }else if(policy.missing<=1e-6){
                    message="Recorded rest goal is met for the latest "+duration(rest.window)+".";
                    sub="Record includes long eye closures, looking away and declared breaks.";
                }else{
                    message="Reminder snoozed";
                    sub="Monitoring continues. Long closures and declared breaks stay separate from ordinary blinks.";
                }
                // Draw the banner and controls, then show the completed dashboard.
                cv::rectangle(canvas,{8,680,1024,70},banner,-1);
                label(canvas,message,18,705,0.58);
                label(canvas,sub,18,732,0.44);
                label(canvas,"SPACE break | P pause | S snooze | R reset | D details | Q / ESC quit",18,775,0.50);
                cv::imshow("Eye Monitor",canvas);
                // Read keyboard input and stop cleanly if the user quits or closes the window.
                int key=cv::waitKey(1);
                if(key=='q'||key=='Q'||key==27)break;
                try{if(cv::getWindowProperty("Eye Monitor",cv::WND_PROP_VISIBLE)<1)break;}
                catch(const cv::Exception&){break;}
                // SPACE toggles declared rest; P interrupts ongoing events; S delays reminders; D toggles details.
                if(key==' '&&!paused)rest.toggleManual(session);
                if(key=='p'||key=='P'){
                    if(rest.manual)rest.toggleManual(session);
                    eyes.pause(session);rest.pauseAway(session);baseline.pause();paused=!paused;
                }
                if(key=='s'||key=='S')rest.snooze(session);
                if(key=='d'||key=='D')details=!details;
                // R starts a fresh session, including counts, learned references and rest history.
                if(key=='r'||key=='R'){
                    eyes=EyeActivity{};baseline=PersonalBaseline{};rest=RestPolicy{};reminder.reset();session=0;paused=false;lastLog=-5;
                }

            }

            // Export exactly five values every five seconds, independently of the camera frame rate.
            if (real - lastJson >= 5.0) {
                bool saved = writeMetricsJson(jsonPath, eyes, session, baseline.baselineRate, rest);
                if (!saved && !jsonFailed) std::cerr << "Cannot update JSON: " << jsonPath.string() << '\n';
                if (saved && jsonFailed) std::cout << "JSON output recovered.\n";
                jsonFailed = !saved;
                lastJson = real;
            }
            // Print diagnostic status every five seconds without adding fields to the JSON file.
            if(showPreview&&session-lastLog>=5&&!paused){
                std::cout<<"STATUS valid="<<eye.valid<<" blink="<<eye.blinkLeft<<'/'<<eye.blinkRight
                         <<" jaw="<<eye.jawOpen<<" pitch="<<eye.pitch<<" yaw="<<eye.yaw<<" state="<<eyes.state<<" blinks="<<eyes.blinks
                         <<" closed_s="<<eyes.totalClosed<<" rest_window_s="<<rest.recordedRest(session,eyes,rest.window)
                         <<" rest20_s="<<rest.recordedRest(session,eyes)<<" away="<<rest.awayActive<<" gaze_away="<<eye.lookingAway<<" coverage="<<policy.coverage<<" fps="<<fps<<'\n'<<std::flush;
                lastLog=session;
            }
        }

        // Save one final snapshot on normal exit, then release the camera and close the window.
        if (!checkFrames && !writeMetricsJson(jsonPath, eyes, session, baseline.baselineRate, rest))
            std::cerr << "Cannot save final JSON.\n";
        camera.release();
        if (showPreview) cv::destroyAllWindows();
        return 0;
    // Report initialization or processing errors and return a failure exit code.
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}


