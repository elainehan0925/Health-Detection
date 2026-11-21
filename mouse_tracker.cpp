#include <windows.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>
using namespace std;

int main(){
    using Clock = chrono::steady_clock;
    auto start = Clock::now();
    bool first = true;
    double distance = 0, moveTime = 0, totalDistance = 0, intervalBegin = 0;
    int count, previousX, previousY, leftClickCount = 0, rightClickCount = 0;
    double previousTime = 0, avgSpeed = 0, instSpeed = 0, acceleration = 0, leftSpeed = 0, rightSpeed = 0, previousSpeed = 0;
    bool previousLeft = false, previousRight = false;
    double lastActivityTime = 0;

    fstream openFile(
        "mouse.json", 
        ios::in | ios::out | ios::trunc
    );
    if(openFile.fail()){
        cout << "Cannot open file" << endl;
        return 1;
    }

    POINT current;
    count = 0;
    openFile << "{\n";
    openFile << "\"intervals\": [\n";
    // Remember where the closing ] begins
    streampos insertPos = openFile.tellp();

    // Close JSON
    openFile << "\n]}\n";
    openFile.flush();
    while(1){
        auto now = Clock::now();
        double elapsed = chrono::duration<double>(now - start).count();
        double dt = elapsed - previousTime;
        
        /*if(elapsed >= 5.0){
            break;
        }*/

        if(GetCursorPos(&current)){
            cout << "Time: " << elapsed << "s " << current.x << " " << current.y;
            if(count == 0){
                previousTime = elapsed;
                cout << endl;
                previousX = current.x;
                previousY = current.y;
                acceleration = 0;
            }else{
                 // record end moving time
                double dx = current.x - previousX;
                double dy = current.y - previousY;
                distance = sqrt(dx * dx + dy * dy);
                
                if(distance > 0){
                    moveTime += dt;
                    lastActivityTime = elapsed;
                }
                cout << " Distance: " << distance ;
                totalDistance += distance;
                
                avgSpeed = totalDistance / elapsed;
                //acceleration = instSpeed / dt;
                if(dt > 0){
                    instSpeed = distance / dt;
                    
                    acceleration = (instSpeed - previousSpeed) / dt;
                    previousSpeed = instSpeed;
                }
                
                cout << " Instantenous speed: " << instSpeed << " Average speed: " << avgSpeed << " Acceleration: " << acceleration << endl;
                previousX = current.x;
                previousY = current.y;
            }
            previousTime = elapsed;
            count++;
            
            //output data into .json file
            //this part is passing data to .json every 50ms, but comment it for now 
            /*if(!first){
                openFile << ",\n";
            }

            openFile << "{"
                     << "\"time\": " << elapsed
                     << ", \"x\": " << current.x
                     << ", \"y\": " << current.y
                     << "}";

            first = false;*/
        }

        bool currentLeft = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

        bool currentRight = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        // Detect left click
        if(currentLeft && !previousLeft){
            leftClickCount++;
            lastActivityTime = elapsed;
            cout << "Left click detected" << endl;
        }

        // Detect right click
        if(currentRight && !previousRight){
            rightClickCount++;
            lastActivityTime = elapsed;
            cout << "Right click detected" << endl;
        }

        // Update previous states
        previousLeft = currentLeft;
        previousRight = currentRight;
        //wait for next record
        this_thread::sleep_for(chrono::milliseconds(50));
        if(elapsed - intervalBegin >= 5.0){
            double duration = elapsed - intervalBegin;
            leftSpeed = leftClickCount / duration;
            rightSpeed = rightClickCount / duration;
            avgSpeed = totalDistance / duration;
            double totalClickSpeed = (leftClickCount + rightClickCount) / duration;
            bool activity =(moveTime > 0 || leftClickCount > 0 || rightClickCount > 0);
            openFile.clear();
            openFile.seekp(insertPos);
            if(!first) openFile << ",\n";
            openFile << "{\n";

            openFile << "\"total_moving_time\": "
                     << moveTime << ",\n";

            openFile << "\"total_distance\": "
                     << totalDistance << ",\n";

            openFile << "\"average_speed\": "
                     << avgSpeed << ",\n";

            openFile << "\"left_clicks\": "
                     << leftClickCount << ",\n";

            openFile << "\"right_clicks\": "
                     << rightClickCount << ",\n";

            openFile << "\"left_click_speed\": "
                     << leftSpeed << ",\n";

            openFile << "\"right_click_speed\": "
                     << rightSpeed << ",\n";

            openFile << "\"total_click_speed\": "
                     << totalClickSpeed << ",\n";
    
            openFile << "\"last_activity_time\": "
                     << lastActivityTime << ",\n";

            openFile << "\"activity\": "
             << (activity ? "true" : "false") << "\n";

            openFile << "}";

            // Finish any buffered output before locating the end
            openFile.flush();

            insertPos = openFile.tellp();

            // Write closing JSON structure
            openFile << "\n]}\n";
            openFile.flush();
            cout << "Total moving time: " << moveTime << " s Total distance: " << totalDistance << endl;
            first = false;

            // Reset counters for the next interval
            intervalBegin = elapsed;
            moveTime = 0;
            totalDistance = 0;
            leftClickCount = 0;
            rightClickCount = 0;
            avgSpeed = 0;
            
            
        }
        if(GetAsyncKeyState(VK_ESCAPE) & 0x8000){
            break;
        }
    }
    openFile.close();
    
    return 0;
}