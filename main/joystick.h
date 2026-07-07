#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "esp_err.h"

#define DIR_NONE  0
#define DIR_UP    1
#define DIR_DOWN  2
#define DIR_LEFT  3
#define DIR_RIGHT 4

#define JOYSTICK_DEFAULT_DEADZONE 450
#define JOYSTICK_CALIBRATION_SAMPLES 20

typedef struct {
    int x;
    int y;
} JoystickReading;

typedef struct {
    int center_x;
    int center_y;
} JoystickCalibration;

esp_err_t joystick_init(void);
esp_err_t joystick_read_raw(JoystickReading *reading);
esp_err_t joystick_calibrate(JoystickCalibration *calibration, int samples);

#endif
