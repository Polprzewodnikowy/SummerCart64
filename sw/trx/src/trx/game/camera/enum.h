#pragma once

typedef enum {
    CAM_CHASE = 0,
    CAM_FIXED = 1,
    CAM_LOOK = 2,
    CAM_COMBAT = 3,
    CAM_CINEMATIC = 4,
    CAM_HEAVY = 5,
    CAM_PHOTO_MODE = 6,
} CAMERA_TYPE;

typedef enum {
    CF_NORMAL = 0,
    CF_FOLLOW_CENTRE = 1,
    CF_NO_CHUNKY = 2,
    CF_CHASE_OBJECT = 3,
} CAMERA_FLAGS;
