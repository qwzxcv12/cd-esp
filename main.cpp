/**
 * ============================================================================
 * DỰ ÁN: TEST TỪNG BÓNG LED MA TRẬN 16x16 FULL RGB (4x HT16D35A)
 * Thư mục: D:\thien\01_DuAn_Code\espc3 - led
 * Framework: ESP-IDF v5.1+ (Build & Flash tự động qua cloud_builder.py)
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI PHẦN CỨNG:
 * - Chân CLK : GPIO 1  (Chân 10 HT16D35A, nối chung 4 IC)
 * - Chân DIO : GPIO 0  (Chân 9 HT16D35A, nối chung 4 IC)
 * - Chân CS1 : GPIO 20 (IC 1: D400..D463)
 * - Chân CS2 : GPIO 21 (IC 2: D500..D563)
 * - Chân CS3 : GPIO 9  (IC 3: D600..D663)
 * - Chân CS4 : GPIO 10 (IC 4: D700..D763)
 * 
 * ÁNH XẠ ĐỊA CHỈ PHẦN CỨNG CHÍNH XÁC 100%:
 * - Ma trận vật lý: 16 cột x 16 hàng (256 bóng LED Full RGB).
 * - 4 IC HT16D35A: Mỗi IC quản lý 4 cột x 16 hàng = 64 bóng LED RGB.
 * - 16 hàng trên mạch chia thành 8 nhóm triplet t = row % 8 (t = 0..7).
 *   + addr_R = t * 3 + 0 (Màu Đỏ)
 *   + addr_G = t * 3 + 1 (Màu Xanh Lá)
 *   + addr_B = t * 3 + 2 (Màu Xanh Dương)
 * - 8 bit COM của mỗi IC:
 *   + COM0..COM3 (bit 0..3): 4 cột của nửa dưới (Hàng 0..7) -> com_bit = col
 *   + COM4..COM7 (bit 4..7): 4 cột của nửa trên (Hàng 8..15) -> com_bit = col + 4
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

static const char *TAG = "RGB_MATRIX_16X16";

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

// Framebuffer toàn cục: 28 bytes cho mỗi IC (24 bytes tích cực: 0..23)
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

    // Xóa DDRAM ban đầu
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
// HÀM ĐIỀU KHIỂN CHÍNH XÁC TỪNG BÓNG LED RGB THEO TỌA ĐỘ VẬT LÝ
// ============================================================================
/**
 * Bật/tắt 1 bóng LED RGB:
 * @param ic: 0 (IC1: D400..), 1 (IC2: D500..), 2 (IC3: D600..), 3 (IC4: D700..)
 * @param col: Cột trong IC (0..3)
 * @param row: Hàng vật lý trên mạch (0 = đáy D400, 15 = đỉnh D415)
 * @param r, g, b: Bật/tắt màu Đỏ, Xanh lá, Xanh dương
 */
static void set_led_rgb(uint8_t ic, uint8_t col, uint8_t row, bool r, bool g, bool b) {
    if (ic >= NUM_ICS || col >= 4 || row >= 16) return;

    // 8 nhóm triplet: t = row % 8
    uint8_t t = row % 8;

    uint8_t addr_R = t * 3 + 0;
    uint8_t addr_G = t * 3 + 1;
    uint8_t addr_B = t * 3 + 2;

    // Hàng 0..7 (nửa dưới): COM0..3 (bit 0..3)
    // Hàng 8..15 (nửa trên): COM4..7 (bit 4..7)
    uint8_t com_bit = (row < 8) ? col : (col + 4);
    uint8_t mask = (1 << com_bit);

    if (r) fb[ic][addr_R] |= mask; else fb[ic][addr_R] &= ~mask;
    if (g) fb[ic][addr_G] |= mask; else fb[ic][addr_G] &= ~mask;
    if (b) fb[ic][addr_B] |= mask; else fb[ic][addr_B] &= ~mask;
}

/**
 * Bật duy nhất 1 bóng LED theo số thứ tự k (0..63) trong IC
 * Silkscreen tăng dần từ đáy lên đỉnh theo từng cột:
 * Cột 0: k=0..15  (D400 đáy đến D415 đỉnh)
 * Cột 1: k=16..31 (D416 đáy đến D431 đỉnh)
 * Cột 2: k=32..47 (D432 đáy đến D447 đỉnh)
 * Cột 3: k=48..63 (D448 đáy đến D463 đỉnh)
 */
static void light_single_led_index(uint8_t ic, uint8_t k) {
    fb_clear();
    uint8_t col = k / 16;  // Cột 0..3
    uint8_t row = k % 16;  // Hàng 0..15 (0 đáy, 15 đỉnh)

    // Bật cả 3 màu = SÁNG TRẮNG ĐẦY ĐỦ CỰC SẮC NÉT
    set_led_rgb(ic, col, row, true, true, true);
    fb_render();
}

// ============================================================================
// BÀI TEST 1: SÁNG TUẦN TỰ TỪNG BÓNG LED THEO ĐÚNG THỨ TỰ SILKSCREEN
// D400 -> D463 (IC 1)
// D500 -> D563 (IC 2)
// D600 -> D663 (IC 3)
// D700 -> D763 (IC 4)
// ============================================================================
static void test_step_by_step_leds(void) {
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "BAT DAU TEST TUNG BONG LED FULL RGB THEO DUNG SILKSCREEN:");
    ESP_LOGI(TAG, "IC 1: D400 -> D463 | IC 2: D500 -> D563");
    ESP_LOGI(TAG, "IC 3: D600 -> D663 | IC 4: D700 -> D763");
    ESP_LOGI(TAG, "==========================================================");

    for (int ic = 0; ic < NUM_ICS; ic++) {
        int base_name = 400 + ic * 100;
        ESP_LOGI(TAG, ">>> BAT DAU IC %d (CS: GPIO %d): D%03d DEN D%03d <<<", 
                 ic + 1, CS_PINS[ic], base_name, base_name + 63);

        for (int k = 0; k < 64; k++) {
            light_single_led_index(ic, k);

            int led_num = base_name + k;
            int chip_col = k / 16;
            int screen_col = ic * 4 + chip_col;
            int row = k % 16;

            ESP_LOGI(TAG, "LED D%03d [IC%d | Col:%02d | Row:%02d] -> ON", led_num, ic + 1, screen_col, row);
            vTaskDelay(pdMS_TO_TICKS(80)); // 80ms mỗi bóng, chạy mượt mà rõ ràng
        }
        vTaskDelay(pdMS_TO_TICKS(500)); // Nghỉ 0.5s giữa các IC
    }
}

// ============================================================================
// BÀI TEST 2: QUÉT TỪNG HÀNG 16 BÓNG TỪ DƯỚI LÊN TRÊN (HÀNG 0 -> 15)
// ============================================================================
static void test_row_sweep(void) {
    ESP_LOGI(TAG, "=== TEST QUET TUNG HANG 16 BONG (DUOI -> TREN) ===");
    for (int r = 0; r < 16; r++) {
        fb_clear();
        for (int x = 0; x < 16; x++) {
            uint8_t ic = x / 4;
            uint8_t col = x % 4;
            set_led_rgb(ic, col, r, true, true, false); // Màu Vàng
        }
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    fb_clear();
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(400));
}

// ============================================================================
// BÀI TEST 3: QUÉT TỪNG CỘT 16 BÓNG TỪ TRÁI SANG PHẢI (CỘT 0 -> 15)
// ============================================================================
static void test_column_sweep(void) {
    ESP_LOGI(TAG, "=== TEST QUET TUNG COT 16 BONG (TRAI -> PHAI) ===");
    for (int x = 0; x < 16; x++) {
        fb_clear();
        uint8_t ic = x / 4;
        uint8_t col = x % 4;
        for (int r = 0; r < 16; r++) {
            set_led_rgb(ic, col, r, false, true, true); // Màu Cyan
        }
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    fb_clear();
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(400));
}

// ============================================================================
// BÀI TEST 4: SÁNG TOÀN BỘ MA TRẬN 16x16 THEO CÁC MÀU (ĐỎ, XANH LÁ, XANH DƯƠNG, TRẮNG)
// ============================================================================
static void test_all_matrix_colors(void) {
    ESP_LOGI(TAG, "=== TEST SANG TOAN BO 256 BONG RGB ===");

    // 1. ĐỎ (addr_R = p * 3 + 0)
    fb_clear();
    for (int ic = 0; ic < NUM_ICS; ic++) {
        for (int p = 0; p < 8; p++) fb[ic][p * 3 + 0] = 0xFF;
    }
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 2. XANH LÁ (addr_G = p * 3 + 1)
    fb_clear();
    for (int ic = 0; ic < NUM_ICS; ic++) {
        for (int p = 0; p < 8; p++) fb[ic][p * 3 + 1] = 0xFF;
    }
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 3. XANH DƯƠNG (addr_B = p * 3 + 2)
    fb_clear();
    for (int ic = 0; ic < NUM_ICS; ic++) {
        for (int p = 0; p < 8; p++) fb[ic][p * 3 + 2] = 0xFF;
    }
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 4. TRẮNG (R + G + B = tất cả 24 bytes)
    fb_clear();
    for (int ic = 0; ic < NUM_ICS; ic++) {
        for (int r = 0; r < 24; r++) fb[ic][r] = 0xFF;
    }
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(1200));

    fb_clear();
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(500));
}

// ============================================================================
// HỆ TỌA ĐỘ 16x16 VÀ VẼ HÌNH
// ============================================================================
static void set_pixel_16x16(int x, int y, bool r, bool g, bool b) {
    if (x < 0 || x >= 16 || y < 0 || y >= 16) return;
    uint8_t ic = x / 4;
    uint8_t col = x % 4;
    uint8_t row = 15 - y; // y=0 là đỉnh (Hàng 15), y=15 là đáy (Hàng 0)
    set_led_rgb(ic, col, row, r, g, b);
}

// Bảng Font 8x8 chuẩn phóng to 2x2 thành 16x16
static const uint8_t FONT_8X8[10][8] = {
    {0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00}, // 0
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // 1
    {0x3C, 0x66, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00}, // 2
    {0x3C, 0x66, 0x0C, 0x1C, 0x0C, 0x66, 0x3C, 0x00}, // 3
    {0x0C, 0x1C, 0x34, 0x64, 0x7E, 0x0C, 0x0C, 0x00}, // 4
    {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00}, // 5
    {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00}, // 6
    {0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00}, // 7
    {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00}, // 8
    {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00}, // 9
};

static void draw_digit_16x16(char digit) {
    fb_clear();
    if (digit < '0' || digit > '9') return;
    const uint8_t *glyph = FONT_8X8[digit - '0'];

    for (int r = 0; r < 8; r++) {
        uint8_t row_val = glyph[r];
        for (int c = 0; c < 8; c++) {
            if (row_val & (1 << (7 - c))) {
                int x0 = c * 2;
                int y0 = r * 2;
                // Màu Xanh Lá đậm nét cho chữ số
                set_pixel_16x16(x0,     y0,     false, true, false);
                set_pixel_16x16(x0 + 1, y0,     false, true, false);
                set_pixel_16x16(x0,     y0 + 1, false, true, false);
                set_pixel_16x16(x0 + 1, y0 + 1, false, true, false);
            }
        }
    }
    fb_render();
}

// ============================================================================
// TASK ĐIỀU KHIỂN CHÍNH
// ============================================================================
static void led_rgb_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== KHOI DONG HE THONG 16x16 FULL RGB (4x HT16D35A) ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    while (1) {
        // BÀI TEST 1: SÁNG TUẦN TỰ TỪNG BÓNG D400->D463, D500->D563, D600->D663, D700->D763
        test_step_by_step_leds();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // BÀI TEST 2: QUÉT TỪNG HÀNG TỪ DƯỚI LÊN TRÊN (HÀNG 0 -> 15)
        test_row_sweep();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // BÀI TEST 3: QUÉT TỪNG CỘT TỪ TRÁI SANG PHẢI (CỘT 0 -> 15)
        test_column_sweep();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // BÀI TEST 4: SÁNG TOÀN BỘ 256 BÓNG VỚI 4 MÀU (ĐỎ, XANH LÁ, XANH DƯƠNG, TRẮNG)
        test_all_matrix_colors();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // BÀI TEST 5: HIỂN THỊ CÁC CHỮ SỐ TỪ 1 ĐẾN 9 CHIẾM TRỌN 16x16
        ESP_LOGI(TAG, "=== TEST HIEN THI SO TO 16x16 (1 DEN 9) ===");
        for (char d = '1'; d <= '9'; d++) {
            ESP_LOGI(TAG, "-> Hien thi So: %c", d);
            draw_digit_16x16(d);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "ESP32-C3 16x16 FULL RGB TEST THEO DUNG SILKSCREEN");
    ESP_LOGI(TAG, "==================================================");
    init_gpio();
    xTaskCreate(led_rgb_task, "led_rgb_task", 4096, NULL, 5, NULL);
}
