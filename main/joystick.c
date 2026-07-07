#include "joystick.h"

#include <stdbool.h>
#include <stdint.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * ESP32-C3 ADC1 channel mapping is board dependent through the selected pins.
 * Defaults:
 * ADC_CHANNEL_0 usually maps to GPIO0.
 * ADC_CHANNEL_1 usually maps to GPIO1.
 *
 * Change these macros if the HW-504 VRx/VRy wires are connected elsewhere.
 */
#define JOYSTICK_ADC_UNIT ADC_UNIT_1
#define JOYSTICK_X_ADC_CHANNEL ADC_CHANNEL_0
#define JOYSTICK_Y_ADC_CHANNEL ADC_CHANNEL_1
#define JOYSTICK_ADC_ATTEN ADC_ATTEN_DB_12
#define JOYSTICK_ADC_BITWIDTH ADC_BITWIDTH_DEFAULT
#define JOYSTICK_CALIBRATION_DELAY_MS 10

static const char *TAG = "joystick";
static adc_oneshot_unit_handle_t s_adc_handle;
static bool s_adc_initialized;

esp_err_t joystick_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = JOYSTICK_ADC_UNIT,
    };

    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config, &s_adc_handle), TAG,
                        "adc unit init failed");

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = JOYSTICK_ADC_ATTEN,
        .bitwidth = JOYSTICK_ADC_BITWIDTH,
    };

    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc_handle, JOYSTICK_X_ADC_CHANNEL,
                                                   &channel_config),
                        TAG, "adc x channel config failed");
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc_handle, JOYSTICK_Y_ADC_CHANNEL,
                                                   &channel_config),
                        TAG, "adc y channel config failed");

    s_adc_initialized = true;
    return ESP_OK;
}

esp_err_t joystick_read_raw(JoystickReading *reading)
{
    if (reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_adc_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(adc_oneshot_read(s_adc_handle, JOYSTICK_X_ADC_CHANNEL, &reading->x),
                        TAG, "adc x read failed");
    ESP_RETURN_ON_ERROR(adc_oneshot_read(s_adc_handle, JOYSTICK_Y_ADC_CHANNEL, &reading->y),
                        TAG, "adc y read failed");

    return ESP_OK;
}

esp_err_t joystick_calibrate(JoystickCalibration *calibration, int samples)
{
    if (calibration == NULL || samples <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int64_t sum_x = 0;
    int64_t sum_y = 0;

    for (int i = 0; i < samples; ++i) {
        JoystickReading reading;
        ESP_RETURN_ON_ERROR(joystick_read_raw(&reading), TAG, "calibration read failed");
        sum_x += reading.x;
        sum_y += reading.y;
        vTaskDelay(pdMS_TO_TICKS(JOYSTICK_CALIBRATION_DELAY_MS));
    }

    calibration->center_x = (int)(sum_x / samples);
    calibration->center_y = (int)(sum_y / samples);

    return ESP_OK;
}
