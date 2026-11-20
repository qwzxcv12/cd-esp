/**
 * ============================================================================
 * DỰ ÁN: SCAN TẤT CẢ 28 ĐỊA CHỈ ROW (0..27) VÀ TOÀN BỘ MA TRẬN 16x16
 * Thư mục: D:\thien\01_DuAn_Code\espc3 - led
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "HT16D35A_SCAN";

// --- CHÂN GPIO ESP32-C3 ---
#define PIN_CLK         GPIO_NUM_1
#define PIN_DIO         GPIO_NUM_0

#define PIN_CS1         GPIO_NUM_20  // IC 1: D400..D463
#define PIN_CS2         GPIO_NUM_21  // IC 2: D500..D563
#define PIN_CS3         GPIO_NUM_9   // IC 3: D600..D663
#define PIN_CS4         GPIO_NUM_10  // IC 4: D700..D763

#define NUM_ICS         4
static const gpio_num_t CS_PINS[NUM_ICS] = {PIN_CS1, PIN_CS2, PIN_CS3, PIN_CS4};

// --- MÃ LỆNH HOLTEK HT16D35A ---
#define CMD_SOFT_RESET          0xCC
#define CMD_SOFT_RESET_ALT      0x00
#define CMD_BINARY_MODE         0x31 // 0x01 = Binary Mode (1 bit/LED)
#define CMD_COM_NUMBER          0x32 // 0x07 = 8 chân COM (COM0..COM7)
#define CMD_CASCADE_MODE        0x34 // 0x01 = Master IRC
#define CMD_SYSTEM_CTRL         0x35 // 0x03 = Osc ON + Display ON
#define CMD_CURRENT_RATIO       0x36 // 0x07 = Dòng cực đại
#define CMD_GLOBAL_BRIGHTNESS   0x37 // 0x3F = Độ sáng tối đa 63/63
#define CMD_FUNCTION_ENABLE     0x38 // 0x00 = Tắt fade/blink
#define CMD_SCROLL_CTRL         0x20 // 0x00 = Tắt cuộn
#define CMD_COM_PIN_CTRL        0x41 // 0xFF = Mở COM0..COM7
#define CMD_ROW_PIN_CTRL        0x42 // 4 bytes 0xFF = Mở ROW0..ROW27
#define CMD_WRITE_DDRAM         0x80 // Ghi dữ liệu vào DDRAM

#define NUM_ROWS_PER_IC  28

// Framebuffer toàn cục: 28 bytes cho mỗi IC
static uint8_t fb[NUM_ICS][NUM_ROWS_PER_IC];

static inline void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

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

static void send_cmd_1b(uint8_t ic_idx, uint8_t cmd) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(4);
    write_byte(cmd);
    delay_us(4);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(4);
}

static void send_cmd_2b(uint8_t ic_idx, uint8_t cmd, uint8_t p1) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(4);
    write_byte(cmd);
    write_byte(p1);
    delay_us(4);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(4);
}

static void send_cmd_5b(uint8_t ic_idx, uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(4);
    write_byte(cmd);
    write_byte(p1);
    write_byte(p2);
    write_byte(p3);
    write_byte(p4);
    delay_us(4);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(4);
}

static void fb_clear(void) {
    memset(fb, 0, sizeof(fb));
}

static void fb_render(void) {
    for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
        gpio_set_level(CS_PINS[ic], 0);
        delay_us(4);
        write_byte(CMD_WRITE_DDRAM);
        write_byte(0x00);
        for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
            write_byte(fb[ic][r]);
        }
        delay_us(4);
        gpio_set_level(CS_PINS[ic], 1);
        delay_us(4);
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

    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(4);
    write_byte(CMD_WRITE_DDRAM);
    write_byte(0x00);
    for (int r = 0; r < NUM_ROWS_PER_IC; r++) write_byte(0x00);
    delay_us(4);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(4);

    send_cmd_2b(ic_idx, CMD_SYSTEM_CTRL, 0x03);
    ESP_LOGI(TAG, "IC %d (CS GPIO %d) khoi tao thanh cong", ic_idx + 1, CS_PINS[ic_idx]);
}

static void init_gpio(void) {
    gpio_reset_pin(PIN_CLK);
    gpio_reset_pin(PIN_DIO);
    gpio_set_direction(PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_CLK, 0);
    gpio_set_level(PIN_DIO, 0);

    for (int i = 0; i < NUM_ICS; i++) {
        gpio_reset_pin(CS_PINS[i]);
        gpio_set_direction(CS_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(CS_PINS[i], 1);
    }
}

// ============================================================================
// BÀI TEST 1: BẬT SÁNG TOÀN BỘ 100% CẢ 28 ĐỊA CHỈ ROW (256 BÓNG) TRÊN 4 IC
// ============================================================================
static void test_turn_all_28_rows_on(void) {
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "TEST 1: SANG TOAN BO TAT CA 28 ROW PINS (0..27) CUA CA 4 IC");
    ESP_LOGI(TAG, "==========================================================");
    fb_clear();
    for (int ic = 0; ic < NUM_ICS; ic++) {
        for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
            fb[ic][r] = 0xFF; // Bật hết tất cả 8 bit COM của tất cả 28 ROW
        }
    }
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(3000)); // Giữ sáng 3 giây để nhìn toàn bộ mạch
    fb_clear();
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(500));
}

// ============================================================================
// BÀI TEST 2: QUÉT TỪNG ĐỊA CHỈ ROW TỪ 0 ĐẾN 27 TRÊN IC 1 (CS1 - GPIO 20)
// Mỗi bước bật cả 8 bit COM (0xFF) của đúng 1 hàng ROW để xác định vị trí
// ============================================================================
static void test_scan_all_28_rows_ic1(void) {
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "TEST 2: QUET DÒ TUNG ROW ADDRESS (0 DEN 27) TREN IC 1");
    ESP_LOGI(TAG, "==========================================================");

    for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
        fb_clear();
        fb[0][r] = 0xFF; // Bật 8 bóng của ROW address này trên IC 1
        fb_render();

        ESP_LOGI(TAG, ">>> IC 1 (D400..D463): ROW ADDRESS = %02d (0x%02X) DANG SANG 8 BONG <<<", r, r);
        vTaskDelay(pdMS_TO_TICKS(400)); // 400ms mỗi hàng
    }

    fb_clear();
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ============================================================================
// BÀI TEST 3: QUÉT ĐỒNG LOẠT TỪNG ROW (0..27) TRÊN CẢ 4 IC CÙNG LÚC
// ============================================================================
static void test_scan_all_28_rows_all_ics(void) {
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "TEST 3: QUET ROW (0..27) DONG LOAT TREN CA 4 IC");
    ESP_LOGI(TAG, "==========================================================");

    for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
        fb_clear();
        for (int ic = 0; ic < NUM_ICS; ic++) {
            fb[ic][r] = 0xFF;
        }
        fb_render();

        ESP_LOGI(TAG, ">>> CA 4 IC: ROW ADDRESS = %02d (0x%02X) DANG SANG <<<", r, r);
        vTaskDelay(pdMS_TO_TICKS(400));
    }

    fb_clear();
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// ============================================================================
// TASK CHÍNH
// ============================================================================
static void led_scan_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== KHOI DONG SCAN HE THONG 4x HT16D35A ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    while (1) {
        // BƯỚC 1: BẬT SÁNG TOÀN BỘ 28 ROW TRÊN CẢ 4 IC (3 GIÂY)
        test_turn_all_28_rows_on();

        // BƯỚC 2: QUÉT TỪNG ROW ADDRESS TỪ 0 ĐẾN 27 TRÊN IC 1 (D400..D463)
        test_scan_all_28_rows_ic1();

        // BƯỚC 3: QUÉT TỪNG ROW ADDRESS TỪ 0 ĐẾN 27 TRÊN CẢ 4 IC
        test_scan_all_28_rows_all_ics();

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "ESP32-C3 HT16D35A FULL 28-ROW HARDWARE SCANNER");
    ESP_LOGI(TAG, "==================================================");
    init_gpio();
    xTaskCreate(led_scan_task, "led_scan_task", 4096, NULL, 5, NULL);
}
