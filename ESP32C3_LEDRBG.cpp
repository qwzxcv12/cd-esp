/**
 * ============================================================================
 * DỰ ÁN: ĐIỀU KHIỂN MA TRẬN LED 16x16 BẰNG ESP32-C3 VỚI 4x HT16D35A
 * Thư mục: D:\thien\01_DuAn_Code\espc3 - led
 * Framework: ESP-IDF v5.1+ (Hỗ trợ build tự động qua cloud_builder.py)
 * ============================================================================
 * 
 * PHẦN CỨNG THỰC TẾ (Xác nhận qua video):
 * - Bo mạch tròn/bát giác tích hợp ma trận LED vuông 16x16 (256 bóng LED).
 * - 4 chip HT16D35A điều khiển 4 góc (hoặc các kênh màu RGB / 2 nửa ma trận).
 * 
 * SƠ ĐỒ ĐẤU NỐI CHÂN ESP32-C3:
 * - CLK -> GPIO 1 (Clock chung cả 4 IC)
 * - DIO -> GPIO 0 (Data chung cả 4 IC)
 * - CS1 -> GPIO 20 (Chip Select IC 1)
 * - CS2 -> GPIO 21 (Chip Select IC 2)
 * - CS3 -> GPIO 9  (Chip Select IC 3)
 * - CS4 -> GPIO 10 (Chip Select IC 4)
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

static const char *TAG = "HT16D35A_16X16";

// --- ĐỊNH NGHĨA CHÂN GPIO ---
#define PIN_CLK         GPIO_NUM_1
#define PIN_DIO         GPIO_NUM_0

#define PIN_CS1         GPIO_NUM_20
#define PIN_CS2         GPIO_NUM_21
#define PIN_CS3         GPIO_NUM_9
#define PIN_CS4         GPIO_NUM_10

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
#define CMD_GLOBAL_BRIGHTNESS   0x37 // 0x3F = Độ sáng tối đa
#define CMD_FUNCTION_ENABLE     0x38 // 0x00 = Tắt fade/blink
#define CMD_SCROLL_CTRL         0x20 // 0x00 = Tắt cuộn
#define CMD_COM_PIN_CTRL        0x41 // 0xFF = Mở COM0..COM7
#define CMD_ROW_PIN_CTRL        0x42 // 4 bytes 0xFF = Mở ROW0..ROW27
#define CMD_WRITE_DDRAM         0x80 // Ghi dữ liệu vào DDRAM

#define NUM_ROWS_PER_IC  28

// Framebuffer toàn cục: Mỗi IC 28 bytes
static uint8_t fb[NUM_ICS][NUM_ROWS_PER_IC];

// Biến điều khiển xoay hướng hiển thị (0: Gốc, 1: Xoay 90 độ, 2: 180 độ, 3: 270 độ)
static int display_rotation = 1; // Mặc định xoay 90 độ để đúng chiều theo video

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
    ESP_LOGI(TAG, "IC %d khoi tao thanh cong", ic_idx + 1);
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
// HỆ TỌA ĐỘ VÀ BỘ ĐỆM ĐIỂM ẢNH MA TRẬN 16x16 (CÓ HỖ TRỢ XOAY HƯỚNG)
// ============================================================================

/**
 * Đặt 1 điểm ảnh (x, y) trên ma trận 16x16 (0..15, 0..15).
 * Có hỗ trợ xoay tọa độ để khớp chuẩn xác hướng nhìn của người dùng.
 */
static void set_pixel_16x16(int x, int y, bool on) {
    if (x < 0 || x >= 16 || y < 0 || y >= 16) return;

    int rx = x;
    int ry = y;

    // Xoay tọa độ
    if (display_rotation == 1) { // Xoay 90 độ theo chiều kim đồng hồ
        rx = 15 - y;
        ry = x;
    } else if (display_rotation == 2) { // 180 độ
        rx = 15 - x;
        ry = 15 - y;
    } else if (display_rotation == 3) { // 270 độ
        rx = y;
        ry = 15 - x;
    }

    uint8_t ic;
    uint8_t lx, ly;

    // Ánh xạ 4 góc (2x2 Quadrants):
    // IC 0: Top-Left (0..7, 0..7)
    // IC 1: Top-Right (8..15, 0..7)
    // IC 2: Bottom-Left (0..7, 8..15)
    // IC 3: Bottom-Right (8..15, 8..15)
    if (ry < 8) {
        if (rx < 8) {
            ic = 0; // IC 1
            lx = rx;
            ly = ry;
        } else {
            ic = 1; // IC 2
            lx = rx - 8;
            ly = ry;
        }
    } else {
        if (rx < 8) {
            ic = 2; // IC 3
            lx = rx;
            ly = ry - 8;
        } else {
            ic = 3; // IC 4
            lx = rx - 8;
            ly = ry - 8;
        }
    }

    if (on) {
        fb[ic][ly] |= (1 << lx);
    } else {
        fb[ic][ly] &= ~(1 << lx);
    }
}

// ============================================================================
// BẢNG FONT CHUẨN 8x8 (PHÓNG ĐẠI 2X THÀNH 16x16 SIÊU TO CỰC RÕ)
// ============================================================================
static const uint8_t FONT_8X8[38][8] = {
    // 0 .. 9 (index 0 .. 9)
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

    // A .. Z (index 10 .. 35)
    {0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00}, // A
    {0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00}, // B
    {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00}, // C
    {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00}, // D
    {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00}, // E
    {0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00}, // F
    {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3A, 0x00}, // G
    {0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00}, // H
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00}, // I
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00}, // J
    {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00}, // K
    {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00}, // L
    {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00}, // M
    {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00}, // N
    {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // O
    {0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00}, // P
    {0x3C, 0x66, 0x66, 0x66, 0x6E, 0x3C, 0x0E, 0x00}, // Q
    {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00}, // R
    {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00}, // S
    {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // T
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00}, // U
    {0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x00}, // V
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // W
    {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00}, // X
    {0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00}, // Y
    {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00}, // Z

    // 36: Space, 37: Trái tim
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18}
};

static int get_font_idx(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
    if (c == '<' || c == '*') return 37;
    return 36;
}

// Vẽ 1 ký tự SIÊU TO 16x16 (chiếm trọn cả 16x16 bóng LED)
static void draw_char_16x16(char c) {
    fb_clear();
    int idx = get_font_idx(c);
    const uint8_t *glyph = FONT_8X8[idx];

    for (int r = 0; r < 8; r++) {
        uint8_t row_val = glyph[r];
        for (int col = 0; col < 8; col++) {
            if (row_val & (1 << (7 - col))) {
                // Khối 2x2 cho mỗi điểm ảnh font 8x8
                int x0 = col * 2;
                int y0 = r * 2;
                set_pixel_16x16(x0,     y0,     true);
                set_pixel_16x16(x0 + 1, y0,     true);
                set_pixel_16x16(x0,     y0 + 1, true);
                set_pixel_16x16(x0 + 1, y0 + 1, true);
            }
        }
    }
    fb_render();
}

// Vẽ khung viền bao quanh toàn bộ ma trận 16x16
static void draw_box_border_16x16(void) {
    fb_clear();
    for (int i = 0; i < 16; i++) {
        set_pixel_16x16(i, 0,  true);
        set_pixel_16x16(i, 15, true);
        set_pixel_16x16(0, i,  true);
        set_pixel_16x16(15, i, true);
    }
    fb_render();
}

// Vẽ mũi tên chỉ lên UP để kiểm tra chiều đứng của bo mạch
static void draw_arrow_up_16x16(void) {
    fb_clear();
    static const uint16_t arrow[16] = {
        0b0000000110000000,
        0b0000001111000000,
        0b0000011111100000,
        0b0000111111110000,
        0b0001111111111000,
        0b0011110110111100,
        0b0111100110011110,
        0b0000000110000000,
        0b0000000110000000,
        0b0000000110000000,
        0b0000000110000000,
        0b0000000110000000,
        0b0000000110000000,
        0b0000000110000000,
        0b0000000110000000,
        0b0000000000000000
    };
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (arrow[y] & (1 << (15 - x))) {
                set_pixel_16x16(x, y, true);
            }
        }
    }
    fb_render();
}

// Hiển thị 4 ký tự nhỏ ở 4 góc: TL, TR, BL, BR
static void draw_4chars_quadrants(char tl, char tr, char bl, char br) {
    fb_clear();
    char chars[4] = {tl, tr, bl, br};
    for (int q = 0; q < 4; q++) {
        int idx = get_font_idx(chars[q]);
        const uint8_t *glyph = FONT_8X8[idx];
        int offset_x = (q == 1 || q == 3) ? 8 : 0;
        int offset_y = (q >= 2) ? 8 : 0;

        for (int r = 0; r < 8; r++) {
            uint8_t row = glyph[r];
            for (int c = 0; c < 8; c++) {
                if (row & (1 << (7 - c))) {
                    set_pixel_16x16(offset_x + c, offset_y + r, true);
                }
            }
        }
    }
    fb_render();
}

// Test chẩn đoán: Bật sáng từng chip độc lập để xác định vùng sáng
static void test_quadrant_identification(void) {
    ESP_LOGI(TAG, "=== TEST SÁNG TỪNG CHIP ĐỘC LẬP (XÁC ĐỊNH VỊ TRÍ) ===");
    for (int i = 0; i < NUM_ICS; i++) {
        ESP_LOGI(TAG, "-> Sáng riêng chip %d (CS: GPIO %d)", i + 1, CS_PINS[i]);
        fb_clear();
        for (int r = 0; r < 16; r++) fb[i][r] = 0xFF; // Bật 16 hàng của chip i
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
    fb_clear();
    fb_render();
}

// Task chính thử nghiệm ma trận 16x16
static void test_matrix_16x16_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== KHOI DONG PHAN CUNG MA TRAN LED 16x16 ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    while (1) {
        // 1. Sáng tuần tự 4 chip để nhận diện phần cứng
        test_quadrant_identification();
        vTaskDelay(pdMS_TO_TICKS(500));

        // 2. Vẽ khung viền vuông 16x16
        ESP_LOGI(TAG, "=== VE KHUNG VIEN 16x16 ===");
        draw_box_border_16x16();
        vTaskDelay(pdMS_TO_TICKS(2000));

        // 3. Mũi tên chỉ lên UP để kiểm tra chiều nhìn
        ESP_LOGI(TAG, "=== VE MUI TEN CHI LEN (ARROW UP) ===");
        draw_arrow_up_16x16();
        vTaskDelay(pdMS_TO_TICKS(2000));

        // 4. HIỂN THỊ CÁC SỐ TỪ 1 ĐẾN 9 SIÊU TO 16x16 (FULL MÀN HÌNH)
        ESP_LOGI(TAG, "=== HIEN THI SO 16x16 TO RO RANG (1 -> 9) ===");
        for (char c = '1'; c <= '9'; c++) {
            ESP_LOGI(TAG, "-> So to 16x16: %c", c);
            draw_char_16x16(c);
            vTaskDelay(pdMS_TO_TICKS(1300));
        }

        // 5. HIỂN THỊ CÁC CHỮ CÁI SIÊU TO 16x16 (A -> Z)
        ESP_LOGI(TAG, "=== HIEN THI CHU CAI 16x16 (A -> Z) ===");
        for (char c = 'A'; c <= 'Z'; c++) {
            ESP_LOGI(TAG, "-> Chu to 16x16: %c", c);
            draw_char_16x16(c);
            vTaskDelay(pdMS_TO_TICKS(800));
        }

        // 6. HIỂN THỊ 4 KÝ TỰ Ở 4 GÓC (1234, 5678, ABCD)
        ESP_LOGI(TAG, "=== 4 SO O 4 GOC (1234, 5678, ABCD) ===");
        draw_4chars_quadrants('1', '2', '3', '4');
        vTaskDelay(pdMS_TO_TICKS(2000));
        draw_4chars_quadrants('5', '6', '7', '8');
        vTaskDelay(pdMS_TO_TICKS(2000));
        draw_4chars_quadrants('A', 'B', 'C', 'D');
        vTaskDelay(pdMS_TO_TICKS(2000));

        // 7. BIỂU TƯỢNG TRÁI TIM 16x16
        ESP_LOGI(TAG, "=== TRAI TIM 16x16 ===");
        draw_char_16x16('<');
        vTaskDelay(pdMS_TO_TICKS(2500));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "ESP32-C3 DIEU KHIEN MA TRAN LED 16x16 (4x HT16D35A)");
    ESP_LOGI(TAG, "==================================================");
    init_gpio();
    xTaskCreate(test_matrix_16x16_task, "test_16x16_task", 4096, NULL, 5, NULL);
}
