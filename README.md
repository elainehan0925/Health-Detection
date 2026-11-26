# UTEK-Programming-2026
Project Overview：
This project is a software prototype developed for the UTEK 2026 programming competition. Our team designed a health monitoring program that tracks the user's behavioural metrics, and estimate the user's systemic fatigue level. The main goal of this project is to help users be aware of their working habits, and get some rest in the scenario that they aren't aware of.

The main metric that is used for evaluate the user's behaviour is called Systemic Fatigue Score. This is a value between 0 and 100. A higher score indicates a higher estimated level of fatigue and physical strain, while a lower score suggests a lower level of fatigue. 

To calculate this score, the team implemented 5 main metrics to test on: mouse usage, keyboard usage, blink reduction, viewing position change, and continuous work level. Each of these metrics are worth 20% of the total score. These measurements are collected and analyzed to identify patterns that may indicate increasing fatigue or repetitive strain.

Installation：
Python: 

Must download additional extensions, by running: pip install opencv-python mediapipe


from pynput import keyboard
import time 
import json
import threading
import os


How to Run：
Download 



Deployment Instructions：



Team Members：
James Meng
Elaine Han
Henry Wang
Aadi Aggarwal

Attributions：
James Meng: designed mouse monitoring, implemented algorithm of mouse and keyboard
Henry Wang: designed blink reduction monitoring, implemented algorithm of blink reduction, viewing position change
Elaine Han: designed keyboard monitoring and implemented the main function to integrate the calculations and monitor outputs
Aadi Aggarwal: designed posture check (determining slump time, and if person if at their desk or not)
