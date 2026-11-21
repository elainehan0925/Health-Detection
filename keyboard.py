from pynput import keyboard
import time 
import json
import threading
import os

#variable declarations
count=0
start_time=time.perf_counter()
error_count=0
type_time=0
interval_s=None
interval_e=0
lock=threading.Lock()
complete_data=[]
typing_speed=0
elapsed_time=0
last_key_pressed_time=0
activity=False

#process at each key press
def on_press(key):
    with lock:
        global count
        global error_count
        global interval_s
        global interval_e
        global type_time

        #update current time
        current=time.perf_counter()
        interval_e=current

        #if key pressed, activity is true 
        activity=True

        #update the last_key_pressed_time to the last key in interval
        if(current>last_key_pressed):
            last_key_pressed=current

        #calculations for the active type time
        #inactive threshold is 0.5s 
        #if interval between characters exceed it, it will be added as 0.5s
        if(type_time==0 and count==0):
            if(interval_e-start_time>0.5):
                type_time+=0.5
            else:
                type_time+=interval_e-start_time
        elif((interval_e-interval_s)<=0.5):
            type_time=type_time+interval_e-interval_s
        else:
            type_time=type_time+0.5
        
        #update the interval start time for next key press
        interval_s=current

        #increment count 
        count=count+1

        #increment error count
        if key==keyboard.Key.backspace:
            error_count=error_count+1
        
#creates listener
listener=keyboard.Listener(on_press=on_press)
listener.start()

while(True):
    #record current time
    current=time.perf_counter()
    end=time.perf_counter()

    #updates the json file at 5 seconds
    if((current-start_time)>=5.0):
        with lock: 
            #update all times to the newest times 
            current=time.perf_counter()
            elapsed_time=current-start_time 

            #calculate typing speed
            typing_speed=count/elapsed_time

            #update start time for next interval
            start_time=current

            #restrict type_time to max 5.0s
            if(type_time>5.0):
                type_time=5.0
            
            #record end time
            end=time.perf_counter()

            #update data for json file 
            data={
                "Number of keys entered: ": count,
                "Number of errors made: ": error_count,
                "Active typing time: ": type_time,
                "Average typing speed: ": typing_speed,
                "Last key press time: ": last_key_pressed_time,
                "Activity: ": activity,
                "Interval end time: ": end

            }
            complete_data.append(data)

            #data is kept for 30 minutes and then popped
            if(len(complete_data)>360):
                complete_data.pop(0)
            
            #refresh all variables for next interval
            count=0
            error_count=0
            type_time=0

        #print to json file
        # temp json used so that while writing, it will not be empty to access    
        with open("keyboard.tmp", "w") as f:
            json.dump(complete_data,f, indent=4)
        os.replace("keyboard.tmp", "keyboard.json")
        print(data)
    time.sleep(0.01)




