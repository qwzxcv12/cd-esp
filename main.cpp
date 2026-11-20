/**
 * ============================================================================
 * DỰ ÁN: TEST TỪNG BÓNG LED TRÊN MA TRẬN 16x16 (4x HT16D35A)
 * Thư mục: D:\thien\01_DuAn_Code\espc3 - led
 * Framework: ESP-IDF v5.1+ (Hỗ trợ build tự động qua cloud_builder.py)
 * ============================================================================
 * 
 * PHẦN CỨNG CHUẨN XÁC 100% THEO SILKSCREEN TRÊN MẠCH:
 * - IC 1 (CS1 - GPIO 20): Quản lý 64 LED từ D400 đến D463 (Cột 0, 1, 2, 3)
 *   + Cột 0: D400 (dưới) -> D415 (trên) (COM0, ROW0..15)
 *   + Cột 1: D416 (dưới) -> D431 (trên) (COM1, ROW0..15)
 *   + Cột 2: D432 (dưới) -> D447 (trên) (COM2, ROW0..15)
 *   + Cột 3: D448 (dưới) -> D463 (trên) (COM3, ROW0..15)
 * 
 * - IC 2 (CS2 - GPIO 21): Quản lý 64 LED từ D500 đến D563 (Cột 4, 5, 6, 7)
 *   + Cột 4: D500 -> D515 (COM0, ROW0..15)
 *   + Cột 5: D516 -> D531 (COM1, ROW0..15)
 *   + Cột 6: D532 -> D547 (COM2, ROW0..15)
 *   + Cột 7: D548 -> D563 (COM3, ROW0..15)
 * 
 * - IC 3 (CS3 - GPIO 9): Quản lý 64 LED từ D600 đến D663 (Cột 8, 9, 10, 11)
 *   + Cột 8:  D600 -> D615 (COM0, ROW0..15)
 *   + Cột 9:  D616 -> D631 (COM1, ROW0..15)
 *   + Cột 10: D632 -> D647 (COM2, ROW0..15)
 *   + Cột 11: D648 -> D663 (COM3, ROW0..15)
 * 
 * - IC 4 (CS4 - GPIO 10): Quản lý 64 LED từ D700 đến D763 (Cột 12, 13, 14, 15)
 *   + Cột 12: D700 -> D715 (COM0, ROW0..15)
 *   + Cột 13: D716 -> D731 (COM1, ROW0..15)
 *   + Cột 14: D732 -> D747 (COM2, ROW0..15)
 *   + Cột 15: D748 -> D763 (COM3, ROW0..15)
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

static const char *TAG = "TEST_LED_16X16";

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
// HÀM BẬT CHÍNH XÁC 1 BÓNG LED THEO TÊN (D400..D463, D500..D563, ...)
// ============================================================================

/**
 * Bật duy nhất 1 bóng LED trên toàn bộ ma trận:
 * @param ic: 0 (IC1: D400..), 1 (IC2: D500..), 2 (IC3: D600..), 3 (IC4: D700..)
 * @param k: Vị trí LED từ 0 đến 63 trong IC đó
 */
static void light_single_led(uint8_t ic, uint8_t k) {
    fb_clear();
    if (ic >= NUM_ICS || k >= 64) return;

    uint8_t col = k / 16;  // Cột trong IC (0..3 tương ứng COM0..COM3)
    uint8_t row = k % 16;  // Hàng trong IC (0..15 tương ứng ROW0..ROW15)

    fb[ic][row] = (1 << col);
    fb_render();
}

// ============================================================================
// BÀI TEST 1: SÁNG TUẦN TỰ TỪNG BÓNG LED THEO ĐÚNG YÊU CẦU CỦA BẠN
// ============================================================================
static void test_step_by_step_leds(void) {
    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, "BAT DAU TEST TUNG BONG LED THEO DUNG THU TU SILKSCREEN:");
    ESP_LOGI(TAG, "IC 1: D400 -> D463 | IC 2: D500 -> D563");
    ESP_LOGI(TAG, "IC 3: D600 -> D663 | IC 4: D700 -> D763");
    ESP_LOGI(TAG, "==========================================================");

    for (int ic = 0; ic < NUM_ICS; ic++) {
        int base_name = 400 + ic * 100;
        ESP_LOGI(TAG, ">>> BAT DAU IC %d (CS: GPIO %d): D%03d DEN D%03d <<<", 
                 ic + 1, CS_PINS[ic], base_name, base_name + 63);

        for (int k = 0; k < 64; k++) {
            light_single_led(ic, k);

            int led_num = base_name + k;
            int chip_col = k / 16;
            int screen_col = ic * 4 + chip_col;
            int row = k % 16;

            ESP_LOGI(TAG, "LED D%03d (IC %d | Cot chip: %d | Cot man hinh: %02d | Hang: %02d)",
                     led_num, ic + 1, chip_col, screen_col, row);

            vTaskDelay(pdMS_TO_TICKS(70)); // Độ trễ 70ms mỗi bóng (dễ theo dõi bằng mắt)
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// ============================================================================
// BÀI TEST 2: SÁNG TỪNG CỘT 16 BÓNG TỪ CỘT 0 ĐẾN CỘT 15
// ============================================================================
static void test_column_sweep(void) {
    ESP_LOGI(TAG, "=== TEST QUET TUNG COT 16 BONG (COT 0 -> 15) ===");
    for (int col = 0; col < 16; col++) {
        fb_clear();
        uint8_t ic = col / 4;
        uint8_t chip_col = col % 4;

        // Bật toàn bộ 16 bóng của cột này
        for (int r = 0; r < 16; r++) {
            fb[ic][r] = (1 << chip_col);
        }
        fb_render();

        int base = 400 + ic * 100 + chip_col * 16;
        ESP_LOGI(TAG, "-> Cot man hinh %02d (IC %d, Cot chip %d): LED D%03d den D%03d",
                 col, ic + 1, chip_col, base, base + 15);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
    fb_clear();
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(400));
}

// ============================================================================
// BÀI TEST 3: SÁNG TOÀN BỘ TỪNG KHỐI IC (MỖI IC 64 BÓNG)
// ============================================================================
static void test_block_each_ic(void) {
    ESP_LOGI(TAG, "=== TEST SANG TOAN BO 64 BONG CUA TUNG IC ===");
    for (int ic = 0; ic < NUM_ICS; ic++) {
        fb_clear();
        int base = 400 + ic * 100;
        ESP_LOGI(TAG, "-> Sang toan bo 64 LED cua IC %d (D%03d den D%03d)", ic + 1, base, base + 63);

        // Bật cả 4 cột (COM0..3 = 0x0F) cho toàn bộ 16 hàng
        for (int r = 0; r < 16; r++) {
            fb[ic][r] = 0x0F;
        }
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
    fb_clear();
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(500));
}

// ============================================================================
// HỆ TỌA ĐỘ 16x16 HOÀN CHỈNH CHO MA TRẬN NÀY
// ============================================================================

/**
 * Đặt điểm ảnh (X, Y) trên hệ tọa độ chuẩn 16x16:
 * X: 0 (Cột D400..D415 bên trái) -> 15 (Cột D748..D763 bên phải)
 * Y: 0 (Hàng trên cùng) -> 15 (Hàng dưới cùng)
 */
static void set_pixel_grid_16x16(int x, int y, bool on) {
    if (x < 0 || x >= 16 || y < 0 || y >= 16) return;

    uint8_t ic = x / 4;
    uint8_t chip_col = x % 4;

    // Trong ảnh: D400 là ở DƯỚI CÙNG, D415 là ở TRÊN CÙNG
    // Nên nếu Y=0 là trên cùng thì ROW = 15 - Y
    uint8_t row = 15 - y;

    if (on) {
        fb[ic][row] |= (1 << chip_col);
    } else {
        fb[ic][row] &= ~(1 << chip_col);
    }
}

// Bảng Font 8x8 chuẩn
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

// Hiển thị số to 16x16 chuẩn xác trên toàn bộ ma trận
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
                set_pixel_grid_16x16(x0,     y0,     true);
                set_pixel_grid_16x16(x0 + 1, y0,     true);
                set_pixel_grid_16x16(x0,     y0 + 1, true);
                set_pixel_grid_16x16(x0 + 1, y0 + 1, true);
            }
        }
    }
    fb_render();
}

// ============================================================================
// TASK ĐIỀU KHIỂN CHÍNH
// ============================================================================
static void led_diag_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== KHOI DONG HE THONG TEST 4x HT16D35A (MA TRAN 16x16) ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    while (1) {
        // BÀI TEST 1: SÁNG TỪNG BÓNG TỪ D400 -> D463, TIẾP ĐẾN D500 -> D563, D600 -> D663, D700 -> D763
        test_step_by_step_leds();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // BÀI TEST 2: QUÉT TỪNG CỘT TỪ TRÁI SANG PHẢI (CỘT 0 -> 15)
        test_column_sweep();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // BÀI TEST 3: SÁNG TOÀN BỘ 64 BÓNG CỦA TỪNG IC ĐỂ KIỂM TRA ĐỦ 4 KHỐI
        test_block_each_ic();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // BÀI TEST 4: HIỂN THỊ CÁC SỐ TO TỪ 1 ĐẾN 9 CHIẾM TRỌN 16x16
        ESP_LOGI(TAG, "=== TEST HIEN THI SO TO 16x16 (1 DEN 9) ===");
        for (char d = '1'; d <= '9'; d++) {
            ESP_LOGI(TAG, "-> Hien thi so to 16x16: %c", d);
            draw_digit_16x16(d);
            vTaskDelay(pdMS_TO_TICKS(1200));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "ESP32-C3 TEST MA TRAN 16x16 THEO DUNG SILKSCREEN");
    ESP_LOGI(TAG, "==================================================");
    init_gpio();
    xTaskCreate(led_diag_task, "led_diag_task", 4096, NULL, 5, NULL);
}
