/**
 * DIAGNOSTIC CHAM: Scan tung DDRAM addr cua IC1
 * Moi buoc: 1 addr bat sang, giu 2 giay
 * Serial log: "BUOC X: addr=Y"
 * Muc tieu: Nhin xem addr nao tuong ung bong nao tren silkscreen
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "SCAN";

#define PIN_CLK  GPIO_NUM_1
#define PIN_DIO  GPIO_NUM_0
#define PIN_CS1  GPIO_NUM_20
#define PIN_CS2  GPIO_NUM_21
#define PIN_CS3  GPIO_NUM_9
#define PIN_CS4  GPIO_NUM_10

#define NUM_ICS 4
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

static void cmd1(int ic, uint8_t c) {
    gpio_set_level(CS_PINS[ic], 0); dly(4); wb(c); dly(4); gpio_set_level(CS_PINS[ic], 1); dly(4);
}
static void cmd2(int ic, uint8_t c, uint8_t p) {
    gpio_set_level(CS_PINS[ic], 0); dly(4); wb(c); wb(p); dly(4); gpio_set_level(CS_PINS[ic], 1); dly(4);
}
static void cmd5(int ic, uint8_t c, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
    gpio_set_level(CS_PINS[ic], 0); dly(4); wb(c); wb(p1); wb(p2); wb(p3); wb(p4); dly(4); gpio_set_level(CS_PINS[ic], 1); dly(4);
}

static void fb_clear(void) { memset(fb, 0, sizeof(fb)); }

static void fb_render(void) {
    for (int ic = 0; ic < NUM_ICS; ic++) {
        gpio_set_level(CS_PINS[ic], 0); dly(4);
        wb(CMD_WRITE_DDRAM); wb(0x00);
        for (int r = 0; r < NUM_ROWS; r++) wb(fb[ic][r]);
        dly(4); gpio_set_level(CS_PINS[ic], 1); dly(4);
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
    // Xoa DDRAM
    gpio_set_level(CS_PINS[ic], 0); dly(4);
    wb(CMD_WRITE_DDRAM); wb(0x00);
    for (int r = 0; r < NUM_ROWS; r++) wb(0x00);
    dly(4); gpio_set_level(CS_PINS[ic], 1); dly(4);
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

static void led_task(void *pv) {
    ESP_LOGI(TAG, "=== DIAGNOSTIC CHAM - CHI IC1 ===");
    for (int i = 0; i < NUM_ICS; i++) init_ic(i);

    while (1) {
        // ============================================================
        // PHASE 1: Scan tung ADDR (0..23) voi full byte 0xFF
        //          => Moi addr sang NHIEU bong, nhin ro mau va vi tri
        //          => 2 giay / buoc
        // ============================================================
        ESP_LOGI(TAG, "--- PHASE 1: Scan addr 0..23, full byte 0xFF ---");
        ESP_LOGI(TAG, "Moi addr sang nhieu bong, ghi lai: addr bao nhieu = hang nao, mau gi");
        vTaskDelay(pdMS_TO_TICKS(3000)); // Cho 3s de san sang

        for (int addr = 0; addr < 24; addr++) {
            fb_clear();
            fb[0][addr] = 0xFF; // Chi IC1
            fb_render();
            ESP_LOGI(TAG, "P1 BUOC %02d: IC1 addr=%02d ON (0xFF) | Ghi: hang nao? mau gi?", addr, addr);
            vTaskDelay(pdMS_TO_TICKS(2000)); // 2 giay moi buoc
        }

        // Tat het
        fb_clear(); fb_render();
        ESP_LOGI(TAG, "--- PHASE 1 XONG ---");
        vTaskDelay(pdMS_TO_TICKS(3000));

        // ============================================================
        // PHASE 2: Chi 3 addr dau (0, 1, 2) voi bit 0 duy nhat
        //          => Xac dinh addr 0 = mau gi, addr 1 = mau gi, addr 2 = mau gi
        //          => 3 giay / buoc
        // ============================================================
        ESP_LOGI(TAG, "--- PHASE 2: addr 0,1,2 voi BIT 0 duy nhat ---");
        ESP_LOGI(TAG, "Xac dinh thu tu R/G/B: addr nao = mau do? xanh la? xanh duong?");
        vTaskDelay(pdMS_TO_TICKS(3000));

        for (int addr = 0; addr < 3; addr++) {
            fb_clear();
            fb[0][addr] = 0x01; // Chi IC1, 1 bit duy nhat
            fb_render();
            ESP_LOGI(TAG, "P2 BUOC %d: IC1 addr=%d, bit=0 | MAU GI? Bong D4XX?", addr, addr);
            vTaskDelay(pdMS_TO_TICKS(3000)); // 3 giay
        }

        // Bat ca 3 cung luc => phai sang TRANG 1 bong
        fb_clear();
        fb[0][0] = 0x01; fb[0][1] = 0x01; fb[0][2] = 0x01;
        fb_render();
        ESP_LOGI(TAG, "P2 BUOC 3: addr 0+1+2 cung luc => 1 bong TRANG hay 3 bong khac nhau?");
        vTaskDelay(pdMS_TO_TICKS(3000));

        fb_clear(); fb_render();
        ESP_LOGI(TAG, "--- PHASE 2 XONG ---");
        vTaskDelay(pdMS_TO_TICKS(3000));

        // ============================================================
        // PHASE 3: Scan tung BIT (0..7) cua addr=0
        //          => Xac dinh bit nao = cot nao
        //          => 2 giay / buoc, 8 buoc
        // ============================================================
        ESP_LOGI(TAG, "--- PHASE 3: Scan bit 0..7 cua addr=0 ---");
        ESP_LOGI(TAG, "Moi bit sang 1 bong: ghi bit bao nhieu = cot silkscreen nao?");
        vTaskDelay(pdMS_TO_TICKS(3000));

        for (int bit = 0; bit < 8; bit++) {
            fb_clear();
            fb[0][0] = (1 << bit); // Chi IC1, addr=0, 1 bit
            fb_render();
            ESP_LOGI(TAG, "P3 BUOC %d: IC1 addr=0, bit=%d (COM%d) | Bong nao? Cot nao?", bit, bit, bit);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }

        fb_clear(); fb_render();
        ESP_LOGI(TAG, "--- PHASE 3 XONG. Lap lai tu dau... ---");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-C3 HT16D35A SLOW DIAGNOSTIC ===");
    init_gpio();
    xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);
}
