#!/usr/bin/env python3

# xxx: problem: FOV calc appears to be wrong. inverted...?

# from https://learnopencv.com/camera-calibration-using-opencv/
# see also https://docs.opencv.org/4.x/dc/dbb/tutorial_py_calibration.html

import cv2
import numpy as np
from math import atan
 
# Defining the dimensions of checkerboard
# (an actual chessboard. this is the count of the internal corners)
CHECKERBOARD = (7,7)
criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
 
# Creating vector to store vectors of 3D points for each checkerboard image
objpoints = []
# Creating vector to store vectors of 2D points for each checkerboard image
imgpoints = [] 

# Defining the world coordinates for 3D points
objp = np.zeros((1, CHECKERBOARD[0] * CHECKERBOARD[1], 3), np.float32)
objp[0,:,:2] = np.mgrid[0:CHECKERBOARD[0], 0:CHECKERBOARD[1]].T.reshape(-1, 2)
prev_img_shape = None

cv2.startWindowThread()
cv2.namedWindow('pv', cv2.WINDOW_NORMAL)

# Extracting path of individual image stored in a given directory
cap = cv2.VideoCapture(0)

print("Press space to capture a frame, q to finish.")

img = None
while True:
    key = cv2.waitKey(16) & 0xFF
    ret, img = cap.read()
    
    if not ret:
        print("Failed to grab frame")
        break
    
    # w = img.shape[1]
    # img = img[:, : w]
    
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    ret  = False
    
    if key == ord(' '):
        # Find the chess board corners
        # If desired number of corners are found in the image then ret = true
        ret, corners = cv2.findChessboardCorners(
            gray,
            CHECKERBOARD,
            cv2.CALIB_CB_ADAPTIVE_THRESH + cv2.CALIB_CB_FAST_CHECK + cv2.CALIB_CB_NORMALIZE_IMAGE
        )
        if not ret:
            print("Bad capture")
    elif key == ord('q'):
        print("capture complete...")
        break
    
    """
    If desired number of corner are detected,
    we refine the pixel coordinates and display 
    them on the images of checker board
    """
    if ret:
        print("Good capture")
        objpoints.append(objp)
        # refining pixel coordinates for given 2d points.
        corners2 = cv2.cornerSubPix(gray, corners, (11,11),(-1,-1), criteria)
         
        imgpoints.append(corners2)
 
        # Draw and display the corners
        img = cv2.drawChessboardCorners(img, CHECKERBOARD, corners2, ret)
        
    cv2.imshow('pv', img)

cap.release()
 
cv2.destroyAllWindows()
 
h, w = img.shape[:2]
 
"""
Performing camera calibration by 
passing the value of known 3D points (objpoints)
and corresponding pixel coordinates of the 
detected corners (imgpoints)
"""
ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(
    objpoints,
    imgpoints,
    gray.shape[::-1],
    None,
    None
)

# this comes as a 1x5 matrix row. get that row:
dist = dist[0]

# the matrix is given in pixels. we want it in unit square coordinates:
aspect = w / float(h)
mtx = np.dot(np.diag([1. / w, 1. / h, 1.]), mtx)
t_x = mtx[0,0]
t_y = mtx[1,1] / aspect

fov_x = 2. * atan(t_x / 2.)
fov_y = 2. * atan(t_y / 2.)
ctr_xy = mtx[0,2], mtx[1,2]

print(f"aspect (w/h): {aspect}")
print(f"fov (avg xy): {(fov_x + fov_y) / 2.} radians")
print(f"center (x,y): {mtx[0,2]}, {mtx[1,2]}")
print(f"  k1, k2, k3: {dist[0]}, {dist[1]}, {dist[4]}")
print(f"      p1, p2: {dist[2]}, {dist[3]}")
print("")
print("  camera mtx: ")
print(mtx)
