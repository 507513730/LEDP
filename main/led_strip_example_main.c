/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "modules/LEDPanel_Driver/driver.h"
#include "driver.h"
#include "modules/Music_Module/audio.h"
#include "fft.h"
static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "MainFunction Booted");

    //driver init
    ESP_LOGI(TAG,"DriverInit");
    initRMT();//taskcreated
    clearPanel();
// 测试全红
    uint8_t test_frame[FRAME_SIZE];
    memset(test_frame, 0, FRAME_SIZE);
    for (int j = 0; j < 32; j++) {
        int idx = (j) * 3;//y * Width + x
        test_frame[idx + 0] = 10; // R
        test_frame[idx + 1] = 0;   // G
        test_frame[idx + 2] = 0;   // B
    }
    submitLEDFrame(test_frame);
    vTaskDelay(pdMS_TO_TICKS(5000));
    clearPanel();

    for (int i = 0; i < 8; i++) {
        int idx = (i * 32 + 0) * 3;
        test_frame[idx + 0] = 0;   // R
        test_frame[idx + 1] = 10; // G
        test_frame[idx + 2] = 0;   // B → 显示绿色，便于区分
    }
    submitLEDFrame(test_frame);
    vTaskDelay(pdMS_TO_TICKS(5000));
    clearPanel();
    //audio init
    initMusic();
    init_microphone();//taskcreated
}