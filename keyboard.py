from pynput import keyboard
import time 
import json
import threading
import os

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


def on_press(key):
    with lock:
        global count
        global error_count
        global interval_s
        global interval_e
        global type_time
        current=time.perf_counter()
        
        interval_e=current
        if(type_time==0 and count==0):
            if(interval_e-start_time>0.5):
                type_time+=0.5
            else:
                type_time+=interval_e-start_time
        elif((interval_e-interval_s)<=0.5):
            type_time=type_time+interval_e-interval_s
        else:
            type_time=type_time+0.5
            
        interval_s=current
        count=count+1
        if key==keyboard.Key.backspace:
            error_count=error_count+1
        
        
listener=keyboard.Listener(on_press=on_press)
listener.start()

while(True):
    current=time.perf_counter()
    end=time.perf_counter()

    if((current-start_time)>=5.0):
        with lock: 
            current=time.perf_counter()
            elapsed_time=current-start_time 
            typing_speed=count/elapsed_time
            start_time=current
            if(type_time>5.0):
                type_time=5.0
            end=time.perf_counter()
            data={
                "Number of keys entered: ": count,
                "Number of errors made: ": error_count,
                "Active typing time: ": type_time,
                "Average typing speed: ": typing_speed,
                "Interval end time: ": end
            }
            complete_data.append(data)
            if(len(complete_data)>360):
                complete_data.pop(0)
            
            count=0
            error_count=0
            type_time=0
        with open("keyboard.tmp", "w") as f:
            json.dump(complete_data,f, indent=4)
        os.replace("keyboard.tmp", "keyboard.json")
        print(data)
    time.sleep(0.01)




