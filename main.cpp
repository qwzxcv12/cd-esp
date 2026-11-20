/**
 * ULTRA SLOW: Chi 4 buoc, moi buoc 15 giay, lap lai mai mai
 * Muc tieu: Xac dinh thu tu R/G/B cua IC HT16D35A
 *
 * BUOC 1: IC1 addr=0 bit=0 → 1 sub-pixel (R?) sang 15s
 * BUOC 2: IC1 addr=1 bit=0 → 1 sub-pixel (G?) sang 15s
 * BUOC 3: IC1 addr=2 bit=0 → 1 sub-pixel (B?) sang 15s
 * BUOC 4: IC1 addr=0+1+2 bit=0 → 1 bong TRANG? hay 3 bong? 15s
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "SLOW";

#define PIN_CLK  GPIO_NUM_1
#define PIN_DIO  GPIO_NUM_0
#define PIN_CS1  GPIO_NUM_20
#define PIN_CS2  GPIO_NUM_21
#define PIN_CS3  GPIO_NUM_9
#define PIN_CS4  GPIO_NUM_10
#define NUM_ICS  4
static const gpio_num_t CS_PINS[NUM_ICS] = {PIN_CS1, PIN_CS2, PIN_CS3, PIN_CS4};

#define CMD_SOFT_RESET        0xCC
#define CMD_SOFT_RESET_ALT    0x00
#define CMD_BINARY_MODE       0x31
#define CMD_COM_NUMBER        0x32
#define CMD_CASCADE_MODE      0x34
#define CMD_SYSTEM_CTRL       0x35
#define CMD_CURRENT_RATIO     0x36
#define CMD_GLOBAL_BRIGHTNESS 0x37
#define CMD_FUNCTION_ENABLE   0x38
#define CMD_SCROLL_CTRL       0x20
#define CMD_COM_PIN_CTRL      0x41
#define CMD_ROW_PIN_CTRL      0x42
#define CMD_WRITE_DDRAM       0x80
#define NUM_ROWS 28

static uint8_t fb[NUM_ICS][NUM_ROWS];

static inline void dly(uint32_t us) { esp_rom_delay_us(us); }
static void wb(uint8_t d) {
    for (int i = 7; i >= 0; i--) {
        gpio_set_level(PIN_CLK, 0);
        gpio_set_level(PIN_DIO, (d >> i) & 1);
        dly(2); gpio_set_level(PIN_CLK, 1); dly(2);
    }
    gpio_set_level(PIN_CLK, 0); dly(1);
}
static void cmd1(int ic, uint8_t c) { gpio_set_level(CS_PINS[ic],0); dly(4); wb(c); dly(4); gpio_set_level(CS_PINS[ic],1); dly(4); }
static void cmd2(int ic, uint8_t c, uint8_t p) { gpio_set_level(CS_PINS[ic],0); dly(4); wb(c); wb(p); dly(4); gpio_set_level(CS_PINS[ic],1); dly(4); }
static void cmd5(int ic, uint8_t c, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) { gpio_set_level(CS_PINS[ic],0); dly(4); wb(c); wb(p1); wb(p2); wb(p3); wb(p4); dly(4); gpio_set_level(CS_PINS[ic],1); dly(4); }

static void fb_clear(void) { memset(fb, 0, sizeof(fb)); }
static void fb_render(void) {
    for (int ic = 0; ic < NUM_ICS; ic++) {
        gpio_set_level(CS_PINS[ic],0); dly(4);
        wb(CMD_WRITE_DDRAM); wb(0x00);
        for (int r = 0; r < NUM_ROWS; r++) wb(fb[ic][r]);
        dly(4); gpio_set_level(CS_PINS[ic],1); dly(4);
    }
}
static void init_ic(int ic) {
    cmd1(ic, CMD_SOFT_RESET); dly(100);
    cmd1(ic, CMD_SOFT_RESET_ALT); vTaskDelay(pdMS_TO_TICKS(10));
    cmd2(ic, CMD_BINARY_MODE, 0x01);
    cmd2(ic, CMD_COM_NUMBER, 0x07);
    cmd2(ic, CMD_CASCADE_MODE, 0x01);
    cmd2(ic, CMD_SYSTEM_CTRL, 0x00);
    cmd2(ic, CMD_CURRENT_RATIO, 0x07);
    cmd2(ic, CMD_GLOBAL_BRIGHTNESS, 0x3F);
    cmd2(ic, CMD_FUNCTION_ENABLE, 0x00);
    cmd2(ic, CMD_SCROLL_CTRL, 0x00);
    cmd2(ic, CMD_COM_PIN_CTRL, 0xFF);
    cmd5(ic, CMD_ROW_PIN_CTRL, 0xFF, 0xFF, 0xFF, 0xFF);
    gpio_set_level(CS_PINS[ic],0); dly(4);
    wb(CMD_WRITE_DDRAM); wb(0x00);
    for (int r = 0; r < NUM_ROWS; r++) wb(0x00);
    dly(4); gpio_set_level(CS_PINS[ic],1); dly(4);
    cmd2(ic, CMD_SYSTEM_CTRL, 0x03);
}
static void init_gpio(void) {
    gpio_reset_pin(PIN_CLK); gpio_reset_pin(PIN_DIO);
    gpio_set_direction(PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_CLK, 0); gpio_set_level(PIN_DIO, 0);
    for (int i = 0; i < NUM_ICS; i++) {
        gpio_reset_pin(CS_PINS[i]);
        gpio_set_direction(CS_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(CS_PINS[i], 1);
    }
}

#define HOLD_MS  15000  // 15 GIAY moi buoc

static void led_task(void *pv) {
    for (int i = 0; i < NUM_ICS; i++) init_ic(i);
    ESP_LOGI(TAG, "READY - moi buoc giu %d giay", HOLD_MS/1000);

    int round = 0;
    while (1) {
        round++;
        ESP_LOGI(TAG, "=== VONG %d ===", round);

        // ---- BUOC 1: addr=0 bit=0 ----
        fb_clear();
        fb[0][0] = 0x01;
        fb_render();
        ESP_LOGI(TAG, "BUOC 1/4: addr=0, bit=0 | MAU GI? BONG D4??");
        vTaskDelay(pdMS_TO_TICKS(HOLD_MS));

        // ---- BUOC 2: addr=1 bit=0 ----
        fb_clear();
        fb[0][1] = 0x01;
        fb_render();
        ESP_LOGI(TAG, "BUOC 2/4: addr=1, bit=0 | MAU GI? BONG D4??");
        vTaskDelay(pdMS_TO_TICKS(HOLD_MS));

        // ---- BUOC 3: addr=2 bit=0 ----
        fb_clear();
        fb[0][2] = 0x01;
        fb_render();
        ESP_LOGI(TAG, "BUOC 3/4: addr=2, bit=0 | MAU GI? BONG D4??");
        vTaskDelay(pdMS_TO_TICKS(HOLD_MS));

        // ---- BUOC 4: addr=0+1+2 bit=0 ----
        fb_clear();
        fb[0][0] = 0x01;
        fb[0][1] = 0x01;
        fb[0][2] = 0x01;
        fb_render();
        ESP_LOGI(TAG, "BUOC 4/4: addr=0+1+2, bit=0 | 1 BONG TRANG hay 3 BONG KHAC NHAU?");
        vTaskDelay(pdMS_TO_TICKS(HOLD_MS));

        // ---- TAT het 3 giay ----
        fb_clear(); fb_render();
        ESP_LOGI(TAG, "--- TAT 3s roi lap lai ---");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

extern "C" void app_main(void) {
    init_gpio();
    xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);
}
