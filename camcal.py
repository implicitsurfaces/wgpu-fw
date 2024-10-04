#!/usr/bin/env python3

# xxx: problem: FOV calc appears to be wrong. inverted...?

# from https://learnopencv.com/camera-calibration-using-opencv/
# see also https://docs.opencv.org/4.x/dc/dbb/tutorial_py_calibration.html

import cv2
import numpy as np
from math import atan
from scipy.spatial.transform import Rotation

# Defining the dimensions of checkerboard
# (an actual chessboard. this is the count of the internal corners)
CHECKERBOARD = (7,7)
criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
do_split = True
n_cams   = 2 if do_split else 1

# list of (objpoints, imgpoints) pairs for each camera
cam_points  = []
cam_windows = []
cam_shapes  = []

cv2.startWindowThread()

for i in range(n_cams):
    # Creating vector to store vectors of 3D points for each checkerboard image
    objpoints = []
    # Creating vector to store vectors of 2D points for each checkerboard image
    imgpoints = []
    cam_points.append((objpoints, imgpoints))
    cam_name = f"cam {i}"
    win = cv2.namedWindow(cam_name, cv2.WINDOW_NORMAL)
    cam_windows.append(cam_name)
    cam_shapes.append(None)

# Defining the world coordinates for 3D points
objp = np.zeros((1, CHECKERBOARD[0] * CHECKERBOARD[1], 3), np.float32)
objp[0,:,:2] = np.mgrid[0:CHECKERBOARD[0], 0:CHECKERBOARD[1]].T.reshape(-1, 2)
prev_img_shape = None

# Extracting path of individual image stored in a given directory
cap = cv2.VideoCapture(0)

print("Press space to capture a frame, q to finish.")

img = None
while True:
    key = cv2.waitKey(16) & 0xFF
    if key == ord('q'):
        print("capture complete...")
        break

    ret, img = cap.read()

    if not ret:
        print("Failed to grab frame")
        break

    if do_split:
        w = img.shape[1] // 2
        l_img = img[:, :w]
        r_img = img[:, w:]
        imgs = [l_img, r_img]
        cam_shapes[0] = l_img.shape
        cam_shapes[1] = r_img.shape
    else:
        imgs = [img]
        cam_shapes[0] = img.shape

    all_good = True
    captured = False
    img_pts  = []
    for i, img in enumerate(imgs):
        if key == ord(' '):
            # use this frame for fitting
            ret      = False
            captured = True
            gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
            # Find the chess board corners
            # If desired number of corners are found in the image then ret = true
            ret, corners = cv2.findChessboardCorners(
                gray,
                CHECKERBOARD,
                cv2.CALIB_CB_ADAPTIVE_THRESH + cv2.CALIB_CB_FAST_CHECK + cv2.CALIB_CB_NORMALIZE_IMAGE
            )
            if ret:
                # refine the pixel coordinates to subpixel resolution
                corners2 = cv2.cornerSubPix(gray, corners, (11,11), (-1,-1), criteria)
                cam_objpts, cam_imgpts = cam_points[i]
                img_pts.append(corners2)

                # Draw and display the corners
                img = cv2.drawChessboardCorners(img, CHECKERBOARD, corners2, ret)
            else:
                all_good = False

        cv2.imshow(cam_windows[i], img)

    if captured:
        print("good capture" if all_good else "bad capture")
        if all_good:
            # append the image points only if all cameras succeeded
            for i, img_pts in enumerate(img_pts):
                cam_objpts, cam_imgpts = cam_points[i]
                cam_objpts.append(objp)
                cam_imgpts.append(img_pts)

cap.release()

cv2.destroyAllWindows()

"""
Performing camera calibration by
passing the value of known 3D points (objpoints)
and corresponding pixel coordinates of the
detected corners (imgpoints)
"""
solutions = []
for i in range(n_cams):
    objpts, imgpts = cam_points[i]
    h, w, channels = cam_shapes[i]
    ret, mtx, dist, rvecs, tvecs = cv2.calibrateCamera(
        objpts,
        imgpts,
        (w, h),
        None,
        None
    )

    # this comes as a 1x5 matrix row. get that row:
    params = dist[0]

    # the matrix is given in pixels. we want it in [0,1]^2 coordinates:
    aspect = w / float(h)
    mtx_01 = np.dot(np.diag([1. / w, 1. / h, 1.]), mtx)
    f_x = mtx_01[0,0]
    f_y = mtx_01[1,1]
    # solved by inverting the matrix formula in camera.wgsl
    fov_x = 2. * atan(     1 / (2. * f_x))
    fov_y = 2. * atan(aspect / (2. * f_y))
    ctr_xy = mtx_01[0,2], mtx_01[1,2]

    print(f"camera {i}:")
    print(f"  aspect (w/h): {aspect}")
    print(f"  fov (avg xy): {(fov_x + fov_y) / 2.} radians")
    print(f"      ... x, y: {fov_x}, {fov_y} radians")
    print(f"  center (x,y): {mtx_01[0,2]}, {mtx_01[1,2]}")
    print(f"    k1, k2, k3: {params[0]}, {params[1]}, {params[4]}")
    print(f"        p1, p2: {params[2]}, {params[3]}")
    print("")
    print( "    camera mtx: ")
    print(mtx_01)

    solutions.append((mtx, dist))

if do_split:
    # calibrate stereo camera configuration
    stereocalibration_flags = cv2.CALIB_FIX_INTRINSIC
    # criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 100, 0.0001)
    obj_points  = cam_points[0][0]
    imgpoints_0 = cam_points[0][1] # left
    imgpoints_1 = cam_points[1][1] # right
    mtx_0, dist_0 = solutions[0]
    mtx_1, dist_1 = solutions[1]
    h,w         = cam_shapes[0][:2] # todo: assumption: both cameras have the same resolution
    ret, CM1, dist1, CM2, dist2, R, T, E, F = cv2.stereoCalibrate(
        obj_points,
        imgpoints_0,
        imgpoints_1,
        mtx_0,
        dist_0,
        mtx_1,
        dist_1,
        (w, h),
        # criteria = criteria,
        flags = stereocalibration_flags
    )

    # R is a matrix which puts a coordinate in cam_0's coordinate system into
    # cam_1's coordinate system. therefore it is the matrix which orients cam_0
    # relative to cam_1.
    #
    # T is a vector which puts a coordinate in cam_0's coordinate system into
    # cam_1's coordinate system. therefore it is the vector which positions cam_0
    # relative to cam_1.

    # opencv camera coordinates are rotated 180 degrees about x. apply the rotation
    # to obtain our own native coords. (opencv is y-down, z-forward. we are y-up, z-backward)
    cv2native = np.array([
        [1, 0, 0],
        [0,-1, 0],
        [0, 0,-1],
    ])

    # thiss could probably be simplified? but I can't take that much A-risk right now
    # (algebraical risk)
    R = np.dot(cv2native, np.dot(R, cv2native))
    T = np.dot(cv2native, T)

    # convert the rotation matrix to a quaternion:
    q = Rotation.from_matrix(R).as_quat()

    # xxx: todo: convert to quaternion
    # xxx: todo: convert from opencv's coordinate system to ours (rotation about +x)
    print("")
    print( "stereo parameters:")
    print( "  q (cam_0's rotation relative to cam_1):")
    print(f"    (i, j, k, w) = {q}")
    print( "  T (cam_0 location relative to cam_1):")
    print(f"    (x, y, z) = {T.ravel()}")
    print(f"error: {ret}")
