# NOTE: ADAPTED CODE FROM "tanisheesh": https://github.com/tanisheesh/PosturePro/tree/main/python-app

#in order to run, need to:
#pip install opencv-python mediapipe
#need to download Google's pose landmarkers and place it in the same working directory
#See here: https://developers.google.com/edge/mediapipe/solutions/vision/pose_landmarker
#To install it, type: 
#https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_lite/float16/1/pose_landmarker_lite.task
#in browswer
#if no webcam is available, change the camera use from webcam to external camera, line 71

#import required libraries
import time
import cv2
import math as m
import mediapipe as mp
import json
from mediapipe.tasks import python
from mediapipe.tasks.python import vision




#computes distance between two 2D points (x1, y1) and (x2, y2)
def findDistance(x1, y1, x2, y2):
    # Pythagorean distance in pixel space: sqrt((dx)^2 + (dy)^2)
    dist = m.sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2)
    return dist


#computes an angle in between the vector (x1, y1) -> (x2, y2)
#and an upward vertical reference line in image coordinates (where y increases downward)
def findAngle(x1, y1, x2, y2):
    #cosine formula using dot product divided by product of vector magnitudes
    theta = m.acos((y2 - y1) * (-y1) / (m.sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2) * y1))
    #converts radians to degrees
    degree = int(180 / m.pi) * theta
    return degree




#counters for consecutive frames matching "good" vs. "bad" posture criteria
good_frames = 0
bad_frames = 0


#color tuples defined in BGR order (Blue, Green, Red), which OpenCV uses by default
red = (50, 50, 255)
green = (127, 255, 0)
yellow = (0, 255, 255)
pink = (255, 0, 255)

font = cv2.FONT_HERSHEY_SIMPLEX

#initialize POSEMODEL using Microsoft required file so the code can recognize humans
#in the camera
POSEMODEL = "pose_landmarker_lite.task"

#start timer
session_start = time.monotonic()

interval = 5
intervalCheck = interval

firstTime = True

#entry point when running this script directly (not when imported as a module)
if __name__ == "__main__":
    
    #camera index 0 uses webcam camera (change to 1 if using external camera)
    cap = cv2.VideoCapture(0)

    #verify the camera opened successfully before reading frames
    if not cap.isOpened():
        print("Error: Could not open video.")
        exit()

    #query capture properties and cast to integers for VideoWriter
    fps = int(cap.get(cv2.CAP_PROP_FPS))
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    frame_size = (width, height)

    #chooses code decoder needed to convert frames into mp4
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    
    # Opens video output file with specified output, code decoder, fps, resolution
    video_output = cv2.VideoWriter('output.mp4', fourcc, fps, frame_size)
    
    #points to POSEMODEL, so the camera can correctly identify poses and parts of the body
    baseoptions = python.BaseOptions(model_asset_path=POSEMODEL)
    
    #tells the camera how to run (receives an ongoing video with one main person)
    options = vision.PoseLandmarkerOptions(base_options=baseoptions, running_mode=vision.RunningMode.VIDEO, num_poses=1)
    
    #starts timer
    start_time = time.monotonic()
    prevTimeMS = -1
    
    #define variables for frames of good/bad posture and time
    timeVal = 0
    totalbad = 0
    totalgood = 0
    
    #define variables for number of frames of very bad / not very bad posture
    totalrllybad = 0
    totalnotrllybad = 0
    
    #boolean set to measure if posture is really bad throughout the video
    rllyBadPosture = False
    
    # Main real-time video processing loop
    with vision.PoseLandmarker.create_from_options(options) as landmarker:
        while True:
            elapsed_sec = time.monotonic() - session_start     
            
            # Read a single frame from the camera
            success, image = cap.read()
            if not success:
                print("Skipping empty frame.")
                # NOTE: a `continue` here would prevent using an empty/None `image` below

            # Convert image from BGR (OpenCV) to RGB (MediaPipe expects RGB input)
            image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)

            # Run MediaPipe pose landmark detection on the RGB frame
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=image_rgb)
            timeMS = int((time.monotonic() - start_time) * 1000)
            if timeMS <= prevTimeMS:
                timeMS = prevTimeMS + 1
            prevTimeMS = timeMS
            result = landmarker.detect_for_video(mp_image, timeMS)
            
            # If no human pose landmarks were detected in this frame, show status and keep looping
            if not result.pose_landmarks:
                cv2.putText(image, "No pose detected", (10, 30), font, 0.9, red, 2)
            else:

            # Shortcut references to detected landmarks and landmark enum indices
                lm = result.pose_landmarks[0]

                # Frame dimensions: h = height (rows), w = width (columns)
                h, w = image.shape[:2]
            
                try:
                    # Convert normalized landmark coordinates [0.0, 1.0] into pixel coordinates
                    lshldrx = int(lm[11].x * w)
                    lshldry = int(lm[11].y * h)
                    rshldrx = int(lm[12].x * w)
                    rshldry = int(lm[12].y * h)
                    learx = int(lm[7].x * w)
                    leary = int(lm[7].y * h)
                    lhipx = int(lm[23].x * w)
                    lhipy = int(lm[23].y * h)

                    # Distance between left and right shoulders in pixels;
                    # used here as a rough side-view alignment check (< 100 px suggests profile/aligned)
                    offset = findDistance(lshldrx, lshldry, rshldrx, rshldry)
                    if offset < 100:
                        cv2.putText(image, str(int(offset)) + ' Aligned', (w - 150, 30), font, 0.9, green, 2)
                    else:
                        cv2.putText(image, str(int(offset)) + ' Not Aligned', (w - 150, 30), font, 0.9, red, 2)

                    # Estimate neck inclination (shoulder -> ear) and torso inclination (hip -> shoulder)
                    neckangle = findAngle(lshldrx, lshldry, learx, leary)
                    torsoangle = findAngle(lhipx, lhipy, lshldrx, lshldry)

                    # Format angle readout string for display
                    angle_text_string = f'Neck : {int(neckangle)}  Torso : {int(torsoangle)}'
                    
                    
                    #check posture thresholds: neck angle < 30 deg and torso angle < 20 deg
                    #as the Rapid Office Strain Assessment mentions the 30 degrees cutoff
                    #https://pubmed.ncbi.nlm.nih.gov/35655178/
                    if neckangle < 30 and torsoangle < 20:
                        #add 1 to the "good posture" variables
                        totalbad = totalbad
                        totalgood += 1
                        totalnotrllybad += 1
                        color = green      
                        #for total frames recorded, reset every 50 seconds
                        if (timeVal) > (fps * interval):
                            totalbad = 0
                            totalgood = 0
                            totalnotrllybad = 0
                            totalrllybad = 0 
                            timeVal = 0 
                        timeVal += 1
                    #check posture thresholds: if neck < 45 and torso < 23 deg
                    elif neckangle < 35 and torsoangle < 23:
                        totalgood = totalgood
                        totalbad += 1
                        totalnotrllybad += 1
                        color = yellow
                        #for total frames recorded, cap at 50 seconds
                        if (timeVal) > (fps * interval):
                            totalgood = 0
                            totalbad = 0
                            totalnotrllybad = 0
                            totalrllybad = 0
                            timeVal = 0
                        timeVal += 1
                    #else, posture is severe
                    else:
                        #for total frames recorded, cap at 50 seconds
                        
                        totalgood = totalgood
                        totalbad += 1
                        totalrllybad += 1
                        if (timeVal) > (fps * interval):
                            totalgood = 0
                            totalbad = 0
                            totalnotrllybad = 0
                            totalrllybad = 0
                            timeVal = 0
                        color = red   
                        timeVal += 1
    
                
                    #check if 50 seconds have passed
                    
                    
                    
                    #on the camera display, show shoulder, hip, ear
                    cv2.circle(image, (lshldrx, lshldry), 7, yellow, -1)
                    cv2.circle(image, (learx, leary), 7, yellow, -1)
                    cv2.circle(image, (rshldrx, rshldry), 7, pink, -1)
                    cv2.circle(image, (lhipx, lhipy), 7, yellow, -1)
                    cv2.line(image, (lshldrx, lshldry), (learx, leary), color, 4)
                    cv2.line(image, (lhipx, lhipy), (lshldrx, lshldry), color, 4)
                
                # Catch and print runtime errors inside landmark/angle processing so the loop doesn't crash
                except Exception as e:
                    print(f"Error: {e}")
            if (int(elapsed_sec)) == (intervalCheck):
                intervalCheck += interval
                #if the time spent with severely bad posture > time spent without,
                #set boolean of rllyBadPosture to true
                if (totalrllybad > (totalnotrllybad)):
                    rllyBadPosture = True
                else:
                    rllyBadPosture = False
                #check for divide by 0 error
                if (totalbad > 0):
                    ratio = round(totalgood / totalbad, 5)
                #if total time spent is 0 for bad, return -1
                #signifying it was all good posture
                elif (totalgood > 0):
                    ratio = -1
                #if both time spent for bad and good is 0 
                #return null, signifying error/nobody was detected
                else:
                    ratio = None
                
                if (totalbad + totalgood) == 0:
                    personThere = False
                else:
                    personThere = True
                #calculate how long the user spent in good and bad posture
                goodTime = totalgood / (fps)
                badTime = (totalbad - totalrllybad) / (fps)
                reallyBadTime = totalrllybad / fps

                #create json file with ratio of good to bad posture
                #time spent in good/bad posture
                #and if the user was in severly bad posture for the
                #majority of the time
                data = {"goodToBad": ratio, "goodTime": round(goodTime, 5), "badTime": round(badTime, 5), "reallybadTime": round(reallyBadTime, 5),  "reallyBadPosture": rllyBadPosture, "personThere": personThere, "time": elapsed_sec}
                jsondata = json.dumps(data, indent=2)
                with open("posture.json", "w", encoding="utf-8") as f:
                    f.write(jsondata + "\n")
                totalgood = 0
                totalbad = 0
                totalnotrllybad = 0
                totalrllybad = 0
                timeVal = 0
            video_output.write(image)
            cv2.imshow("MediaPipe Pose", image)
            
            # Poll keyboard for 5 ms; break out of loop if 'q' is pressed
            if cv2.waitKey(5) & 0xFF == ord('q'):
                break

    # Clean up camera, video writer, and GUI windows upon exiting the loop
    cap.release()
    video_output.release()
    cv2.destroyAllWindows()
