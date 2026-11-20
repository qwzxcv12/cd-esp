/**
 * ============================================================================
 * TEST DIAGNOSTIC: SCAN TUNG ADDR x TUNG BIT - IC1 ONLY
 * Muc tieu: Xac dinh chinh xac DDRAM addr va COM bit nao tuong ung bong nao
 * 
 * Moi buoc: Dat 1 bit duy nhat vao 1 addr duy nhat cua IC1
 * Scan: addr 0..23, bit 0..7
 * => Moi buoc chi sang 1 sub-pixel (R, G, hoac B cua 1 bong)
 * => Nhin video + so sanh silkscreen => biet mapping chinh xac
 * ============================================================================
 * HARDWARE:
 * CLK=GPIO1, DIO=GPIO0
 * CS1=GPIO20 (IC1: D400-D463)
 * CS2=GPIO21 (IC2: D500-D563)
 * CS3=GPIO9  (IC3: D600-D663)
 * CS4=GPIO10 (IC4: D700-D763)
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "DIAG";

#define PIN_CLK         GPIO_NUM_1
#define PIN_DIO         GPIO_NUM_0
#define PIN_CS1         GPIO_NUM_20
#define PIN_CS2         GPIO_NUM_21
#define PIN_CS3         GPIO_NUM_9
#define PIN_CS4         GPIO_NUM_10

#define NUM_ICS         4
static const gpio_num_t CS_PINS[NUM_ICS] = {PIN_CS1, PIN_CS2, PIN_CS3, PIN_CS4};

#define CMD_SOFT_RESET          0xCC
#define CMD_SOFT_RESET_ALT      0x00
#define CMD_BINARY_MODE         0x31
#define CMD_COM_NUMBER          0x32
#define CMD_CASCADE_MODE        0x34
#define CMD_SYSTEM_CTRL         0x35
#define CMD_CURRENT_RATIO       0x36
#define CMD_GLOBAL_BRIGHTNESS   0x37
#define CMD_FUNCTION_ENABLE     0x38
#define CMD_SCROLL_CTRL         0x20
#define CMD_COM_PIN_CTRL        0x41
#define CMD_ROW_PIN_CTRL        0x42
#define CMD_WRITE_DDRAM         0x80

#define NUM_ROWS_PER_IC  28

static uint8_t fb[NUM_ICS][NUM_ROWS_PER_IC];

static inline void delay_us(uint32_t us) { esp_rom_delay_us(us); }

static void write_byte(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        gpio_set_level(PIN_CLK, 0);
        gpio_set_level(PIN_DIO, (data & (1 << i)) ? 1 : 0);
        delay_us(2);
        gpio_set_level(PIN_CLK, 1);
        delay_us(2);
    }
    gpio_set_level(PIN_CLK, 0);
    delay_us(1);
}

static void send_cmd_1b(uint8_t ic, uint8_t cmd) {
    gpio_set_level(CS_PINS[ic], 0); delay_us(4);
    write_byte(cmd);
    delay_us(4); gpio_set_level(CS_PINS[ic], 1); delay_us(4);
}

static void send_cmd_2b(uint8_t ic, uint8_t cmd, uint8_t p1) {
    gpio_set_level(CS_PINS[ic], 0); delay_us(4);
    write_byte(cmd); write_byte(p1);
    delay_us(4); gpio_set_level(CS_PINS[ic], 1); delay_us(4);
}

static void send_cmd_5b(uint8_t ic, uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
    gpio_set_level(CS_PINS[ic], 0); delay_us(4);
    write_byte(cmd); write_byte(p1); write_byte(p2); write_byte(p3); write_byte(p4);
    delay_us(4); gpio_set_level(CS_PINS[ic], 1); delay_us(4);
}

static void fb_clear(void) { memset(fb, 0, sizeof(fb)); }

static void fb_render(void) {
    for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
        gpio_set_level(CS_PINS[ic], 0); delay_us(4);
        write_byte(CMD_WRITE_DDRAM); write_byte(0x00);
        for (int r = 0; r < NUM_ROWS_PER_IC; r++) write_byte(fb[ic][r]);
        delay_us(4); gpio_set_level(CS_PINS[ic], 1); delay_us(4);
    }
}

static void init_ht16d35a(uint8_t ic_idx) {
    send_cmd_1b(ic_idx, CMD_SOFT_RESET);
    delay_us(100);
    send_cmd_1b(ic_idx, CMD_SOFT_RESET_ALT);
    vTaskDelay(pdMS_TO_TICKS(10));

    send_cmd_2b(ic_idx, CMD_BINARY_MODE, 0x01);
    send_cmd_2b(ic_idx, CMD_COM_NUMBER, 0x07);
    send_cmd_2b(ic_idx, CMD_CASCADE_MODE, 0x01);
    send_cmd_2b(ic_idx, CMD_SYSTEM_CTRL, 0x00);
    send_cmd_2b(ic_idx, CMD_CURRENT_RATIO, 0x07);
    send_cmd_2b(ic_idx, CMD_GLOBAL_BRIGHTNESS, 0x3F);
    send_cmd_2b(ic_idx, CMD_FUNCTION_ENABLE, 0x00);
    send_cmd_2b(ic_idx, CMD_SCROLL_CTRL, 0x00);
    send_cmd_2b(ic_idx, CMD_COM_PIN_CTRL, 0xFF);
    send_cmd_5b(ic_idx, CMD_ROW_PIN_CTRL, 0xFF, 0xFF, 0xFF, 0xFF);

    gpio_set_level(CS_PINS[ic_idx], 0); delay_us(4);
    write_byte(CMD_WRITE_DDRAM); write_byte(0x00);
    for (int r = 0; r < NUM_ROWS_PER_IC; r++) write_byte(0x00);
    delay_us(4); gpio_set_level(CS_PINS[ic_idx], 1); delay_us(4);

    send_cmd_2b(ic_idx, CMD_SYSTEM_CTRL, 0x03);
    ESP_LOGI(TAG, "IC %d init OK", ic_idx + 1);
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

// ============================================================================
// TEST A: Scan tung ADDR (0..23) voi toan bo 8 bit bat
//         => Moi addr bật mấy bóng? Màu gì? Hàng nào?
//         => 400ms/step, chu ky 24 buoc
// ============================================================================
static void test_A_scan_addr_full(void) {
    ESP_LOGI(TAG, "=== TEST A: Scan addr 0..23 (full byte 0xFF) - IC1 ===");
    for (int addr = 0; addr < 24; addr++) {
        fb_clear();
        fb[0][addr] = 0xFF; // Chi IC1, 1 addr, full byte
        fb_render();
        ESP_LOGI(TAG, "A: IC1 addr=%02d (0x%02X) ON", addr, 0xFF);
        vTaskDelay(pdMS_TO_TICKS(400));
    }
    fb_clear(); fb_render();
    vTaskDelay(pdMS_TO_TICKS(600));
}

// ============================================================================
// TEST B: Chi set 1 BIT duy nhat (bit 0) cho tung addr 0..23 - IC1
//         => Xac dinh chinh xac addr nao = mau gi (R/G/B) cua bong nao
//         => 500ms/step
// ============================================================================
static void test_B_scan_addr_bit0(void) {
    ESP_LOGI(TAG, "=== TEST B: Scan addr 0..23, bit=0 (COM0) - IC1 ===");
    for (int addr = 0; addr < 24; addr++) {
        fb_clear();
        fb[0][addr] = 0x01; // Chi IC1, bit 0 (COM0), 1 sub-pixel
        fb_render();
        ESP_LOGI(TAG, "B: IC1 addr=%02d bit=0 ON", addr);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    fb_clear(); fb_render();
    vTaskDelay(pdMS_TO_TICKS(600));
}

// ============================================================================
// TEST C: 3 addr dau tien (0,1,2) voi bit 0 lan luot
//         => Xac dinh R/G/B order cua triplet dau tien
//         => Moi addr 1s de quan sat mau ro rang
// ============================================================================
static void test_C_rgb_order(void) {
    ESP_LOGI(TAG, "=== TEST C: RGB order check - addr 0,1,2 voi bit0 - IC1 ===");
    for (int addr = 0; addr < 3; addr++) {
        fb_clear();
        fb[0][addr] = 0x01;
        fb_render();
        const char* label = (addr==0) ? "addr0 (gia dinh R?)" : (addr==1) ? "addr1 (gia dinh G?)" : "addr2 (gia dinh B?)";
        ESP_LOGI(TAG, "C: IC1 %s - Nhin mau bong D400 khu vuc", label);
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
    // Tat ca 3 addr => phai sang TRANG neu 3 addr cung 1 bong
    fb_clear();
    fb[0][0] = 0x01; fb[0][1] = 0x01; fb[0][2] = 0x01;
    fb_render();
    ESP_LOGI(TAG, "C: addr 0+1+2 cung bit0 => phai sang TRANG 1 bong");
    vTaskDelay(pdMS_TO_TICKS(1200));
    fb_clear(); fb_render();
    vTaskDelay(pdMS_TO_TICKS(600));
}

// ============================================================================
// TEST D: Scan tung BIT (0..7) cua addr=0 - IC1
//         => Xac dinh bit nao = cot nao (COM0..7)
//         => 600ms/step, 8 buoc
// ============================================================================
static void test_D_scan_bits(void) {
    ESP_LOGI(TAG, "=== TEST D: Scan bit 0..7 cua addr=0 - IC1 ===");
    for (int bit = 0; bit < 8; bit++) {
        fb_clear();
        fb[0][0] = (1 << bit);
        fb_render();
        ESP_LOGI(TAG, "D: IC1 addr=0, bit=%d (COM%d) ON", bit, bit);
        vTaskDelay(pdMS_TO_TICKS(600));
    }
    fb_clear(); fb_render();
    vTaskDelay(pdMS_TO_TICKS(600));
}

// ============================================================================
// TEST E: Sang tung bong theo thu tu TUYEN TINH don gian
//         k=0..63 => addr=(k/4)*3, bit=k%4
//         => Thu xem neu map nhu vay co dung khong
// ============================================================================
static void test_E_linear_simple(void) {
    ESP_LOGI(TAG, "=== TEST E: Linear simple map - IC1 (64 bong) ===");
    // Map: Moi 4 bong lien tiep dung cung triplet, bit 0..3
    // k=0..3 => triplet 0 (addr 0,1,2), bit 0,1,2,3
    // k=4..7 => triplet 1 (addr 3,4,5), bit 0,1,2,3
    // ...tuc la: col = k%4, triplet = k/4
    // => set ca 3 mau cung luc
    for (int k = 0; k < 64; k++) {
        fb_clear();
        int triplet = k / 4; // 0..15 => can 16 triplet nhung chi co 8... => thu
        int bit = k % 4;
        if (triplet < 8) {
            fb[0][triplet*3+0] = (1 << bit);
            fb[0][triplet*3+1] = (1 << bit);
            fb[0][triplet*3+2] = (1 << bit);
        }
        fb_render();
        int base = 400 + k;
        ESP_LOGI(TAG, "E: k=%02d D%03d triplet=%d bit=%d", k, base, triplet < 8 ? triplet : -1, bit);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    fb_clear(); fb_render();
    vTaskDelay(pdMS_TO_TICKS(600));
}

// ============================================================================
// TASK CHINH
// ============================================================================
static void led_rgb_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== DIAGNOSTIC SCAN - 4x HT16D35A ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    while (1) {
        // TEST A: Moi addr full byte => xem phan vung sang
        test_A_scan_addr_full();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // TEST B: Moi addr bit0 => xem chinh xac 1 sub-pixel
        test_B_scan_addr_bit0();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // TEST C: So sanh 3 addr dau (R? G? B?) cung bit0
        test_C_rgb_order();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // TEST D: Scan 8 bit cua addr=0
        test_D_scan_bits();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // TEST E: Thu map don gian tuyen tinh
        test_E_linear_simple();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-C3 HT16D35A DIAGNOSTIC ===");
    init_gpio();
    xTaskCreate(led_rgb_task, "led_rgb_task", 4096, NULL, 5, NULL);
}
