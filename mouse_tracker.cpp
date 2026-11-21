#include <windows.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cmath>
using namespace std;

//the main output that i want to collect: 
//every 
int main(){
    using Clock = chrono::steady_clock;
    auto start = Clock::now();
    bool first = true;
    double distance = 0, moveTime = 0, totalDistance = 0;
    int count, previousX, previousY, leftClickCount = 0, rightClickCount = 0;
    double previousTime = 0, avgSpeed = 0, instSpeed = 0, acceleration = 0, leftSpeed = 0, rightSpeed = 0, previousSpeed = 0;
    bool previousLeft = false, previousRight = false;
    ofstream openFile("mouse_data.json");
    if(openFile.fail()){
        cout << "Cannot open file" << endl;
        return 1;
    }

    POINT current;
    count = 0;
    while(1){
        auto now = Clock::now();
        double elapsed = chrono::duration<double>(now - start).count();
        double dt = elapsed - previousTime;

        if(elapsed >= 5.0){
            break;
        }

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
            cout << "Left click detected" << endl;
            
        }

        // Detect right click
        if(currentRight && !previousRight){
            rightClickCount++;
            cout << "Right click detected" << endl;
            
        }

        // Update previous states
        previousLeft = currentLeft;
        previousRight = currentRight;
        //wait for next record
        this_thread::sleep_for(chrono::milliseconds(50));
    }
    double duration = chrono::duration<double>(Clock::now() - start).count();
    leftSpeed = leftClickCount / duration;
    rightSpeed = rightClickCount / duration;
    double totalClickSpeed = (leftClickCount + rightClickCount) / duration;
    if(!first){
        openFile << ",\n";
    }
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
             << totalClickSpeed << "\n";

    openFile << "}\n";

    openFile.close();
    first = false;
    cout << "Total moving time: " << moveTime << " s Total distance: " << totalDistance << endl;
    
    return 0;
}
