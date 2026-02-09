/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* I2S Digital Microphone Recording Example */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "fft.h"

static const char *TAG = "pdm_rec_example";

#define FFT_SAMPLE_SIZE     2048
//#define NUM_CHANNELS        (1) // For mono recording only!
#define SAMPLE_SIZE         (CONFIG_EXAMPLE_BIT_SAMPLE * FFT_SAMPLE_SIZE)
#define I2S_READ_LEN        (FFT_SAMPLE_SIZE * sizeof(int32_t)) 
#define BYTE_RATE           (CONFIG_EXAMPLE_SAMPLE_RATE * (CONFIG_EXAMPLE_BIT_SAMPLE / 8)) * NUM_CHANNELS
#define INMP_SD     GPIO_NUM_4
#define INMP_SCK    GPIO_NUM_7
#define INMP_WS     GPIO_NUM_6
#define SAMPLE_RATE 44100

i2s_chan_handle_t rx_handle = NULL;

static int32_t i2s_readraw_buff[FFT_SAMPLE_SIZE];
__attribute__((aligned(16))) static float fft_buff[FFT_SAMPLE_SIZE];
size_t bytes_read;

int32_t* getAudioPointer(){
    return i2s_readraw_buff;
}

float* getFFTPointer(){
    return fft_buff;
}

void audio_input_task(void *pvParameters)
{
    size_t bytes_read;
    const float scale = 1.0f / 8388608.0f;
    float avg_dc = 0.0;

    while (1) {
        // 从 I2S 读取原始 PCM 数据（阻塞直到填满 buffer）
        esp_err_t ret = i2s_channel_read(
            rx_handle,
            (char *)i2s_readraw_buff, // 目标缓冲区
            I2S_READ_LEN,             // 要读取的字节数
            &bytes_read,              // 实际读取字节数
            portMAX_DELAY             // 永不超时（实时流必需）
        );
        for(int i = 0;i < N_SAMPLES;i++){
            avg_dc += i2s_readraw_buff[i];
        }
        avg_dc /= N_SAMPLES;
        if (ret == ESP_OK && bytes_read == I2S_READ_LEN) {
            // 转换为 float [-1.0, 1.0]
            //ESP_LOGE(TAG,"ori:%u,%u,%u,%u",i2s_readraw_buff[0],i2s_readraw_buff[1],i2s_readraw_buff[2],i2s_readraw_buff[03]);
            for (int i = 0; i < FFT_SAMPLE_SIZE; i++) {
                fft_buff[i] = ((int32_t)(i2s_readraw_buff[i] - avg_dc) >> 8) * scale;
            }
            //ESP_LOGE(TAG,"ori:%.2f,%.2f,%.2f,%.2f",fft_buff[0],fft_buff[1],fft_buff[2],fft_buff[3]);
            flash_audio_to_arrow(fft_buff);

        } else {
            //ESP_LOGE(TAG, "I2S read failed!");
            vTaskDelay(pdMS_TO_TICKS(10)); // 防止死循环
        }
    }
}

void init_microphone(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    
    //dma frame num使用最大值，增大dma一次搬运的数据量，能够提高效率，减小杂音，使用1023可以做到没有一丝杂音
    chan_cfg.dma_frame_num = 1023;
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);
 
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        
        //虽然inmp441采集数据为24bit，但是仍可使用32bit来接收，中间存储过程不需考虑，只要让声音怎么进来就怎么出去即可
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .dout = I2S_GPIO_UNUSED,
            .bclk = INMP_SCK,
            .ws = INMP_WS,
            .din = INMP_SD,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
 
    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);

    xTaskCreate(
        audio_input_task,
        "audio_viz",
        4096*2,               // 堆栈大小（FFT + 日志可能需要较大）
        NULL,
        configMAX_PRIORITIES - 2, // 较高优先级（避免音频卡顿）
        NULL
    );
}