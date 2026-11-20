/**
 * ============================================================================
 * DỰ ÁN: ĐIỀU KHIỂN MA TRẬN LED 4x HT16D35A BẰNG ESP32-C3
 * Thư mục: D:\thien\01_DuAn_Code\espc3 - led
 * Framework: ESP-IDF v5.1+ (Hỗ trợ build tự động qua cloud_builder.py)
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI ESP32-C3 -> MẠCH 4x HT16D35A:
 * - GND          -> GND ESP32-C3 & Mass nguồn 5V ngoài
 * - VDD (Logic)  -> 3.3V ESP32-C3
 * - LED_VDD      -> Dương nguồn 5V công suất ngoài (Chân 14, 21 trên IC)
 * - CLK          -> GPIO 1 (Chân 10 trên IC, nối chung cả 4 IC)
 * - DIO          -> GPIO 0 (Chân 9 trên IC, nối chung cả 4 IC)
 * - CS1          -> GPIO 20 (Chân 11 của IC 1)
 * - CS2          -> GPIO 21 (Chân 11 của IC 2)
 * - CS3          -> GPIO 9  (Chân 11 của IC 3)
 * - CS4          -> GPIO 10 (Chân 11 của IC 4)
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

static const char *TAG = "HT16D35A_ESP32C3";

// --- ĐỊNH NGHĨA CHÂN GPIO CHO ESP32-C3 ---
#define PIN_CLK         GPIO_NUM_1   // Xung nhịp Clock (Chân 10)
#define PIN_DIO         GPIO_NUM_0   // Dữ liệu Data (Chân 9)

#define PIN_CS1         GPIO_NUM_20  // Chip Select IC 1
#define PIN_CS2         GPIO_NUM_21  // Chip Select IC 2
#define PIN_CS3         GPIO_NUM_9   // Chip Select IC 3
#define PIN_CS4         GPIO_NUM_10  // Chip Select IC 4

#define NUM_ICS         4
static const gpio_num_t CS_PINS[NUM_ICS] = {PIN_CS1, PIN_CS2, PIN_CS3, PIN_CS4};

// --- BẢNG MÃ LỆNH CHUẨN HOLTEK HT16D35A (Datasheet Holtek) ---
#define CMD_SOFT_RESET          0xCC // Reset mềm chính
#define CMD_SOFT_RESET_ALT      0x00 // Reset mềm phụ
#define CMD_BINARY_MODE         0x31 // 0x01 = Chế độ Binary Mode (1 bit/LED)
#define CMD_COM_NUMBER          0x32 // 0x07 = Bật đủ 8 chân COM (COM0..COM7)
#define CMD_CASCADE_MODE        0x34 // 0x01 = IRC Master độc lập từng chip
#define CMD_SYSTEM_CTRL         0x35 // 0x03 = Osc ON + Display ON, 0x00 = Standby
#define CMD_CURRENT_RATIO       0x36 // 0x07 = Dòng quét cực đại 8/8
#define CMD_GLOBAL_BRIGHTNESS   0x37 // 0x3F = Độ sáng PWM tối đa 63/63
#define CMD_FUNCTION_ENABLE     0x38 // 0x00 = Tắt fade/blink để hiển thị trực tiếp
#define CMD_SCROLL_CTRL         0x20 // 0x00 = Tắt cuộn phần cứng
#define CMD_COM_PIN_CTRL        0x41 // 0xFF = Mở công suất COM0..COM7
#define CMD_ROW_PIN_CTRL        0x42 // 4 bytes 0xFF = Mở công suất ROW0..ROW27
#define CMD_WRITE_DDRAM         0x80 // Ghi dữ liệu vào DDRAM (Bắt đầu từ địa chỉ 0x00)

// Cấu hình ma trận RAM trong Binary Mode:
// Mỗi IC có 28 hàng (ROW 0..27). Mỗi hàng gồm 1 byte (8 bits cho COM 0..7).
#define NUM_ROWS_PER_IC  28
#define TOTAL_COLS       (NUM_ICS * NUM_ROWS_PER_IC) // 112 cột khi đặt ngang

// Framebuffer toàn cục: Đúng 28 bytes cho mỗi IC (Tổng cộng 112 bytes)
static uint8_t fb[NUM_ICS][NUM_ROWS_PER_IC];

// Hàm trễ microsecond
static inline void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

// Gửi 1 byte theo giao thức 3-wire SPI (MSB first, chốt ở cạnh lên CLK)
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

// Gửi lệnh 1 byte
static void send_cmd_1b(uint8_t ic_idx, uint8_t cmd) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(4);
    write_byte(cmd);
    delay_us(4);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(4);
}

// Gửi lệnh 2 bytes
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

// Gửi lệnh 5 bytes (ROW PIN CTRL 0x42)
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

// Đảo ngược thứ tự bit (dùng khi chân COM bị ngược chiều)
static inline uint8_t rev_byte(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

// Xóa sạch Framebuffer
static void fb_clear(void) {
    memset(fb, 0, sizeof(fb));
}

// Điền toàn bộ Framebuffer
static void fb_fill(uint8_t pattern) {
    memset(fb, pattern, sizeof(fb));
}

// Gửi Framebuffer xuống cả 4 IC HT16D35A: Đúng chuẩn 28 bytes/IC!
static void fb_render(void) {
    for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
        gpio_set_level(CS_PINS[ic], 0);
        delay_us(4);
        write_byte(CMD_WRITE_DDRAM); // Lệnh ghi DDRAM (0x80)
        write_byte(0x00);            // Bắt đầu từ địa chỉ 0x00 (ROW 0)
        for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
            write_byte(fb[ic][r]);   // Đúng 28 bytes từ 0x00 đến 0x1B
        }
        delay_us(4);
        gpio_set_level(CS_PINS[ic], 1);
        delay_us(4);
    }
}

// Cài đặt độ sáng toàn cục PWM (0..63) cho cả 4 IC
static void set_global_brightness(uint8_t level) {
    if (level > 63) level = 63;
    for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
        send_cmd_2b(ic, CMD_GLOBAL_BRIGHTNESS, level);
    }
}

// Khởi tạo một IC HT16D35A ở chế độ chuẩn Binary Mode
static void init_ht16d35a(uint8_t ic_idx) {
    // 1. Reset mềm IC
    send_cmd_1b(ic_idx, CMD_SOFT_RESET);
    delay_us(100);
    send_cmd_1b(ic_idx, CMD_SOFT_RESET_ALT);
    vTaskDelay(pdMS_TO_TICKS(10));

    // 2. Chuyển sang chế độ BINARY MODE (0x31 + 0x01)
    send_cmd_2b(ic_idx, CMD_BINARY_MODE, 0x01);

    // 3. Cài đặt đủ 8 chân COM: COM0..COM7 (0x32 + 0x07)
    send_cmd_2b(ic_idx, CMD_COM_NUMBER, 0x07);

    // 4. Cascade Mode IRC Master (0x34 + 0x01)
    send_cmd_2b(ic_idx, CMD_CASCADE_MODE, 0x01);

    // 5. Tạm dừng System Control để cấu hình (0x35 + 0x00)
    send_cmd_2b(ic_idx, CMD_SYSTEM_CTRL, 0x00);

    // 6. Dòng quét cực đại 8/8 (0x36 + 0x07)
    send_cmd_2b(ic_idx, CMD_CURRENT_RATIO, 0x07);

    // 7. Độ sáng PWM toàn cục cực đại 63/63 (0x37 + 0x3F)
    send_cmd_2b(ic_idx, CMD_GLOBAL_BRIGHTNESS, 0x3F);

    // 8. Tắt fade/blink/mask để hiển thị trực tiếp (0x38 + 0x00)
    send_cmd_2b(ic_idx, CMD_FUNCTION_ENABLE, 0x00);

    // 9. Tắt chức năng cuộn màn hình phần cứng (0x20 + 0x00)
    send_cmd_2b(ic_idx, CMD_SCROLL_CTRL, 0x00);

    // 10. Mở công suất toàn bộ cổng COM: 0x41 -> 0xFF (COM0..COM7)
    send_cmd_2b(ic_idx, CMD_COM_PIN_CTRL, 0xFF);

    // 11. Mở công suất toàn bộ cổng ROW: 0x42 -> 0xFF, 0xFF, 0xFF, 0xFF (ROW0..ROW27)
    send_cmd_5b(ic_idx, CMD_ROW_PIN_CTRL, 0xFF, 0xFF, 0xFF, 0xFF);

    // 12. Xóa sạch RAM hiển thị (Ghi 28 bytes 0x00 vào địa chỉ 0x00)
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(4);
    write_byte(CMD_WRITE_DDRAM);
    write_byte(0x00);
    for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
        write_byte(0x00);
    }
    delay_us(4);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(4);

    // 13. Bật bộ dao động hệ thống và bật hiển thị (0x35 + 0x03)
    send_cmd_2b(ic_idx, CMD_SYSTEM_CTRL, 0x03);

    ESP_LOGI(TAG, "IC %d: Khoi tao thanh cong Binary Mode (28 rows x 8 COMs)", ic_idx + 1);
}

// Cấu hình các chân GPIO của ESP32-C3
static void init_gpio(void) {
    gpio_reset_pin(PIN_CLK);
    gpio_reset_pin(PIN_DIO);
    gpio_set_direction(PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_CLK, 0); // CLK nghỉ ở mức LOW
    gpio_set_level(PIN_DIO, 0);

    for (int i = 0; i < NUM_ICS; i++) {
        gpio_reset_pin(CS_PINS[i]);
        gpio_set_direction(CS_PINS[i], GPIO_MODE_OUTPUT);
        gpio_set_level(CS_PINS[i], 1); // CS ở mức cao = không chọn chip
    }
}

// ============================================================================
// BẢNG FONT CHỮ & CHỮ SỐ CHUẨN 8x8 (NÉT ĐẬM TO RÕ RÀNG)
// ============================================================================

// 1. Font dạng Cột (Column-Major): Phù hợp cho hiển thị Ngang (112 cột x 8 hàng)
// Mỗi byte tương ứng 1 cột 8 điểm ảnh (bit 0 = hàng trên cùng, bit 7 = hàng dưới cùng)
static const uint8_t FONT_COL_8X8[40][8] = {
    {0x00, 0x3E, 0x7F, 0x49, 0x45, 0x7F, 0x3E, 0x00}, // '0' (idx 0)
    {0x00, 0x40, 0x42, 0x7F, 0x7F, 0x40, 0x40, 0x00}, // '1' (idx 1)
    {0x00, 0x62, 0x73, 0x59, 0x4D, 0x47, 0x42, 0x00}, // '2' (idx 2)
    {0x00, 0x22, 0x63, 0x49, 0x5D, 0x7F, 0x22, 0x00}, // '3' (idx 3)
    {0x00, 0x18, 0x1C, 0x16, 0x73, 0x7F, 0x10, 0x00}, // '4' (idx 4)
    {0x00, 0x27, 0x67, 0x45, 0x45, 0x7D, 0x39, 0x00}, // '5' (idx 5)
    {0x00, 0x3E, 0x7F, 0x49, 0x49, 0x7B, 0x32, 0x00}, // '6' (idx 6)
    {0x00, 0x03, 0x03, 0x79, 0x7D, 0x07, 0x03, 0x00}, // '7' (idx 7)
    {0x00, 0x36, 0x7F, 0x49, 0x49, 0x7F, 0x36, 0x00}, // '8' (idx 8)
    {0x00, 0x26, 0x6F, 0x49, 0x49, 0x7F, 0x3E, 0x00}, // '9' (idx 9)
    {0x00, 0x7E, 0x7F, 0x09, 0x09, 0x7F, 0x7E, 0x00}, // 'A' (idx 10)
    {0x00, 0x7F, 0x7F, 0x49, 0x49, 0x7F, 0x36, 0x00}, // 'B' (idx 11)
    {0x00, 0x3E, 0x7F, 0x41, 0x41, 0x63, 0x22, 0x00}, // 'C' (idx 12)
    {0x00, 0x7F, 0x7F, 0x41, 0x63, 0x3E, 0x1C, 0x00}, // 'D' (idx 13)
    {0x00, 0x7F, 0x7F, 0x49, 0x49, 0x49, 0x41, 0x00}, // 'E' (idx 14)
    {0x00, 0x7F, 0x7F, 0x09, 0x09, 0x09, 0x01, 0x00}, // 'F' (idx 15)
    {0x00, 0x3E, 0x7F, 0x41, 0x49, 0x3B, 0x7A, 0x00}, // 'G' (idx 16)
    {0x00, 0x7F, 0x7F, 0x08, 0x08, 0x7F, 0x7F, 0x00}, // 'H' (idx 17)
    {0x00, 0x41, 0x41, 0x7F, 0x7F, 0x41, 0x41, 0x00}, // 'I' (idx 18)
    {0x00, 0x20, 0x60, 0x41, 0x7F, 0x3F, 0x01, 0x00}, // 'J' (idx 19)
    {0x00, 0x7F, 0x7F, 0x1C, 0x36, 0x63, 0x41, 0x00}, // 'K' (idx 20)
    {0x00, 0x7F, 0x7F, 0x40, 0x40, 0x40, 0x40, 0x00}, // 'L' (idx 21)
    {0x00, 0x7F, 0x7F, 0x06, 0x0C, 0x06, 0x7F, 0x7F}, // 'M' (idx 22)
    {0x00, 0x7F, 0x7F, 0x0E, 0x1C, 0x7F, 0x7F, 0x00}, // 'N' (idx 23)
    {0x00, 0x3E, 0x7F, 0x41, 0x41, 0x7F, 0x3E, 0x00}, // 'O' (idx 24)
    {0x00, 0x7F, 0x7F, 0x09, 0x09, 0x0F, 0x06, 0x00}, // 'P' (idx 25)
    {0x00, 0x1E, 0x3F, 0x21, 0x71, 0x7F, 0x5E, 0x00}, // 'Q' (idx 26)
    {0x00, 0x7F, 0x7F, 0x19, 0x39, 0x6F, 0x46, 0x00}, // 'R' (idx 27)
    {0x00, 0x26, 0x6F, 0x49, 0x49, 0x7B, 0x32, 0x00}, // 'S' (idx 28)
    {0x00, 0x01, 0x01, 0x7F, 0x7F, 0x01, 0x01, 0x00}, // 'T' (idx 29)
    {0x00, 0x3F, 0x7F, 0x40, 0x40, 0x7F, 0x3F, 0x00}, // 'U' (idx 30)
    {0x00, 0x0F, 0x3F, 0x70, 0x70, 0x3F, 0x0F, 0x00}, // 'V' (idx 31)
    {0x00, 0x7F, 0x7F, 0x30, 0x18, 0x30, 0x7F, 0x7F}, // 'W' (idx 32)
    {0x00, 0x63, 0x77, 0x1C, 0x1C, 0x77, 0x63, 0x00}, // 'X' (idx 33)
    {0x00, 0x07, 0x0F, 0x78, 0x78, 0x0F, 0x07, 0x00}, // 'Y' (idx 34)
    {0x00, 0x61, 0x71, 0x59, 0x4D, 0x47, 0x43, 0x00}, // 'Z' (idx 35)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' ' (idx 36)
    {0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}, // '-' (idx 37)
    {0x00, 0x00, 0x00, 0x36, 0x36, 0x00, 0x00, 0x00}, // ':' (idx 38)
    {0x00, 0x00, 0x00, 0x5F, 0x5F, 0x00, 0x00, 0x00}, // '!' (idx 39)
};

// 2. Font dạng Hàng (Row-Major): Phù hợp cho hiển thị Đứng (32 cột x 28 hàng)
// Mỗi byte tương ứng 1 hàng 8 điểm ảnh
static const uint8_t FONT_ROW_8X8[40][8] = {
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
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00}, // '-'
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}, // ':'
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00}, // '!'
};

static int get_char_index(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
    if (c == '-') return 37;
    if (c == ':') return 38;
    if (c == '!') return 39;
    return 36; // Khoảng trắng ' '
}

// ============================================================================
// HÀM VẼ KÝ TỰ THEO HƯỚNG NGANG (HORIZONTAL: 112 CỘT x 8 HÀNG)
// ============================================================================

/**
 * Vẽ 1 ký tự TO RÕ RÀNG căn giữa trên 1 IC (28 cột).
 * @param ic: Chỉ số IC (0..3)
 * @param c: Ký tự cần vẽ ('0'..'9', 'A'..'Z')
 * @param scale: Tỉ lệ phóng to chiều ngang (1 = chuẩn 8 cột, 2 = to đậm 16 cột, 3 = cực to 24 cột)
 * @param rev_com: Đảo ngược thứ tự COM (true nếu chữ bị lộn ngược đầu)
 */
static void draw_char_H(uint8_t ic, char c, uint8_t scale, bool rev_com) {
    if (ic >= NUM_ICS) return;
    int idx = get_char_index(c);
    const uint8_t *cols = FONT_COL_8X8[idx];

    // Xóa sạch IC này
    for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
        fb[ic][r] = 0;
    }

    int total_w = 8 * scale;
    if (total_w > NUM_ROWS_PER_IC) total_w = NUM_ROWS_PER_IC;
    int start_col = (NUM_ROWS_PER_IC - total_w) / 2; // Căn chính giữa 28 cột của IC

    for (int col_i = 0; col_i < 8; col_i++) {
        uint8_t col_val = cols[col_i];
        if (rev_com) col_val = rev_byte(col_val);

        for (int s = 0; s < scale; s++) {
            int target_col = start_col + col_i * scale + s;
            if (target_col < NUM_ROWS_PER_IC) {
                fb[ic][target_col] = col_val;
            }
        }
    }
}

// Hiển thị chuỗi 4 ký tự to trên 4 IC (Mỗi IC 1 ký tự to)
static void display_4chars_H(const char *str, uint8_t scale, bool rev_com) {
    fb_clear();
    int len = strlen(str);
    for (int i = 0; i < NUM_ICS; i++) {
        if (i < len && str[i] != '\0') {
            draw_char_H(i, str[i], scale, rev_com);
        }
    }
    fb_render();
}

// ============================================================================
// HÀM VẼ KÝ TỰ THEO HƯỚNG ĐỨNG (VERTICAL: 32 CỘT x 28 HÀNG)
// ============================================================================

/**
 * Vẽ 1 ký tự TO RÕ trên 1 IC (khi bo mạch đặt thẳng đứng: 8 cột x 28 hàng).
 * Phóng to 3x theo chiều dọc (chiếm 24 hàng trên 28 hàng).
 */
static void draw_char_V(uint8_t ic, char c, bool rev_bits) {
    if (ic >= NUM_ICS) return;
    int idx = get_char_index(c);
    const uint8_t *rows = FONT_ROW_8X8[idx];

    for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
        fb[ic][r] = 0;
    }

    // Phóng to 3x chiều dọc: 8 hàng font -> 24 hàng (hàng 2 đến 25)
    for (int r = 0; r < 8; r++) {
        uint8_t val = rows[r];
        if (rev_bits) val = rev_byte(val);
        int base = 2 + r * 3;
        if (base + 2 < NUM_ROWS_PER_IC) {
            fb[ic][base]     = val;
            fb[ic][base + 1] = val;
            fb[ic][base + 2] = val;
        }
    }
}

static void display_4chars_V(const char *str, bool rev_bits) {
    fb_clear();
    int len = strlen(str);
    for (int i = 0; i < NUM_ICS; i++) {
        if (i < len && str[i] != '\0') {
            draw_char_V(i, str[i], rev_bits);
        }
    }
    fb_render();
}

// ============================================================================
// CHỨC NĂNG CUỘN CHỮ MARQUEE MƯỢT MÀ QUA CẢ 112 CỘT (HORIZONTAL SCROLL)
// ============================================================================
static void scroll_text_H(const char *text, int speed_ms, bool rev_com) {
    ESP_LOGI(TAG, ">>> MARQUEE SCROLL: \"%s\"", text);
    int text_len = strlen(text);
    int char_w = 8; // Mỗi ký tự 8 cột (có khoảng cách đệm sẵn)
    int total_text_cols = text_len * char_w;

    // Buffer chứa toàn bộ dòng chữ
    uint8_t *stream = (uint8_t *)calloc(total_text_cols, sizeof(uint8_t));
    if (!stream) return;

    for (int i = 0; i < text_len; i++) {
        int idx = get_char_index(text[i]);
        const uint8_t *cols = FONT_COL_8X8[idx];
        for (int c = 0; c < 8; c++) {
            uint8_t val = cols[c];
            if (rev_com) val = rev_byte(val);
            stream[i * char_w + c] = val;
        }
    }

    // Cuộn từ mép phải màn hình sang hết mép trái
    int max_scroll = total_text_cols + TOTAL_COLS;
    for (int offset = 0; offset < max_scroll; offset++) {
        fb_clear();
        for (int screen_col = 0; screen_col < TOTAL_COLS; screen_col++) {
            int t_idx = offset - (TOTAL_COLS - screen_col);
            if (t_idx >= 0 && t_idx < total_text_cols) {
                uint8_t ic = screen_col / NUM_ROWS_PER_IC;
                uint8_t row = screen_col % NUM_ROWS_PER_IC;
                fb[ic][row] = stream[t_idx];
            }
        }
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(speed_ms));
    }

    free(stream);
}

// ============================================================================
// CHỨC NĂNG CHẨN ĐOÁN CHIỀU VÀ TỌA ĐỘ PHẦN CỨNG (DIAGNOSTIC TEST)
// ============================================================================
static void diagnostic_sweep(void) {
    ESP_LOGI(TAG, "=== TEST CHAN DOAN TOA DO & CHIEN QUET MA TRAN LED ===");

    // 1. Sáng tuần tự từng chip từ IC 1 đến IC 4 (mỗi chip 600ms)
    for (int i = 0; i < NUM_ICS; i++) {
        ESP_LOGI(TAG, "-> Kiem tra sang tron ven IC %d", i + 1);
        fb_clear();
        for (int r = 0; r < NUM_ROWS_PER_IC; r++) fb[i][r] = 0xFF;
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(600));
    }
    fb_clear();
    fb_render();
    vTaskDelay(pdMS_TO_TICKS(300));

    // 2. Quét 1 vạch sáng chạy ngang qua 112 cột từ cột 0 đến cột 111
    ESP_LOGI(TAG, "-> Quet 1 vach sang chay qua 112 cot");
    for (int c = 0; c < TOTAL_COLS; c += 2) {
        fb_clear();
        uint8_t ic = c / NUM_ROWS_PER_IC;
        uint8_t r = c % NUM_ROWS_PER_IC;
        fb[ic][r] = 0xFF;
        if (r + 1 < NUM_ROWS_PER_IC) fb[ic][r + 1] = 0xFF;
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    fb_clear();
    fb_render();
}

// ============================================================================
// CÁC BÀI TEST CHỮ SỐ & CHỮ CÁI TO RÕ RÀNG
// ============================================================================

// Test 1: Đếm các số to từ 1 đến 9 (Phóng to 2x bề ngang, nét đôi cực rõ)
static void test_digits_1_to_9_big(bool rev_com) {
    ESP_LOGI(TAG, "=== 1. DEM SO TO TU 1 DEN 9 (PHONG TO 2X NET DOI DAM) ===");
    for (char c = '1'; c <= '9'; c++) {
        ESP_LOGI(TAG, "-> So to: %c", c);
        char buf[5] = {c, c, c, c, '\0'};
        display_4chars_H(buf, 2, rev_com); // Scale 2x
        vTaskDelay(pdMS_TO_TICKS(1100));
    }
}

// Test 2: Đếm các số to cực đại 3x (Phóng to 24 cột chiếm trọn IC)
static void test_digits_1_to_9_huge(bool rev_com) {
    ESP_LOGI(TAG, "=== 2. SO TO CUC DAI 3X (24 COT CHIEM TRON IC) ===");
    for (char c = '1'; c <= '9'; c++) {
        ESP_LOGI(TAG, "-> So cuc to 3x: %c", c);
        char buf[5] = {c, c, c, c, '\0'};
        display_4chars_H(buf, 3, rev_com); // Scale 3x
        vTaskDelay(pdMS_TO_TICKS(900));
    }
}

// Test 3: Nhóm 4 số và chữ cái (Mỗi IC 1 ký tự khác nhau)
static void test_groups_4chars(bool rev_com) {
    ESP_LOGI(TAG, "=== 3. NHOM 4 SO VA CHU CAI TO TRUC QUAN ===");
    const char *groups[] = {
        "1234", "5678", "9012",
        "ABCD", "EFGH", "IJKL",
        "MNOP", "QRST", "UVWX", "YZ-!"
    };
    int n = sizeof(groups) / sizeof(groups[0]);
    for (int i = 0; i < n; i++) {
        ESP_LOGI(TAG, "-> Nhom ky tu: %s", groups[i]);
        display_4chars_H(groups[i], 2, rev_com);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

// Test 4: Chữ cái to A -> Z tuần tự
static void test_letters_A_to_Z(bool rev_com) {
    ESP_LOGI(TAG, "=== 4. CHU CAI TO TU A DEN Z ===");
    for (char c = 'A'; c <= 'Z'; c++) {
        ESP_LOGI(TAG, "-> Chu cai to: %c", c);
        char buf[5] = {c, c, c, c, '\0'};
        display_4chars_H(buf, 2, rev_com);
        vTaskDelay(pdMS_TO_TICKS(600));
    }
}

// Test 5: Hiển thị chế độ dọc (Dành cho mạch xoay đứng 32x28)
static void test_vertical_mode(void) {
    ESP_LOGI(TAG, "=== 5. CHE DO DUNG (VERTICAL 8x24 TALL CHO MOI IC) ===");
    const char *groups[] = {"1234", "5678", "ABCD", "WXYZ"};
    int n = sizeof(groups) / sizeof(groups[0]);
    for (int i = 0; i < n; i++) {
        ESP_LOGI(TAG, "-> Chu dung: %s", groups[i]);
        display_4chars_V(groups[i], false);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

// ============================================================================
// HIỆU ỨNG HÌNH ẢNH TRÌNH DIỄN (VISUAL EFFECTS)
// ============================================================================

// Hiệu ứng sóng ánh sáng quét qua lại 112 cột
static void effect_wave_scan(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG: SONG ANH SANG QUET (SCANNING WAVE)");
    int steps = duration_ms / 25;
    int head = 0;
    int dir = 1;
    for (int s = 0; s < steps; s++) {
        fb_clear();
        for (int t = 0; t < 5; t++) {
            int c = head - t * dir;
            if (c >= 0 && c < TOTAL_COLS) {
                uint8_t ic = c / NUM_ROWS_PER_IC;
                uint8_t r = c % NUM_ROWS_PER_IC;
                uint8_t mask = (t == 0) ? 0xFF : (t == 1 ? 0x7E : (t == 2 ? 0x3C : 0x18));
                fb[ic][r] = mask;
            }
        }
        fb_render();
        head += dir;
        if (head >= TOTAL_COLS - 1) dir = -1;
        else if (head <= 0) dir = 1;
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

// Hiệu ứng bàn cờ caro đảo pha (Checkerboard Alternating)
static void effect_checkerboard(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG: BAN CO CARO DAO PHA");
    int steps = duration_ms / 250;
    for (int step = 0; step < steps; step++) {
        fb_clear();
        uint8_t phase = step % 2;
        for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
            for (uint8_t r = 0; r < NUM_ROWS_PER_IC; r++) {
                uint8_t col_pattern = 0;
                for (int bit = 0; bit < 8; bit++) {
                    if ((r + bit + ic + phase) % 2 == 0) {
                        col_pattern |= (1 << bit);
                    }
                }
                fb[ic][r] = col_pattern;
            }
        }
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

// Task chạy toàn bộ chương trình thử nghiệm
static void led_test_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== KHOI TAO PHAN CUNG 4x HT16D35A (BINARY MODE) ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    set_global_brightness(63); // Độ sáng cực đại

    while (1) {
        // 0. Chẩn đoán vị trí & quét vạch sáng xác định chiều
        diagnostic_sweep();
        vTaskDelay(pdMS_TO_TICKS(500));

        // 1. Đếm các số to từ 1 đến 9 (Phóng to 2x nét đôi cực rõ ràng)
        test_digits_1_to_9_big(false);
        vTaskDelay(pdMS_TO_TICKS(800));

        // 2. Đếm các số cực to 3x (Chiếm trọn bề ngang IC)
        test_digits_1_to_9_huge(false);
        vTaskDelay(pdMS_TO_TICKS(800));

        // 3. Hiển thị từng nhóm 4 số và chữ cái (1234, 5678, ABCD, EFGH...)
        test_groups_4chars(false);
        vTaskDelay(pdMS_TO_TICKS(800));

        // 4. Hiển thị chữ cái to A -> Z
        test_letters_A_to_Z(false);
        vTaskDelay(pdMS_TO_TICKS(800));

        // 5. Cuộn chữ Marquee mượt mà qua cả 112 cột
        scroll_text_H("1 2 3 4 5 6 7 8 9 - A B C D E F G H I J K L M N O P Q R S T U V W X Y Z ", 35, false);
        vTaskDelay(pdMS_TO_TICKS(800));

        // 6. Thử nghiệm chế độ xoay dọc (dành cho màn hình đặt đứng)
        test_vertical_mode();
        vTaskDelay(pdMS_TO_TICKS(800));

        // 7. Các hiệu ứng hình ảnh
        effect_wave_scan(3500);
        effect_checkerboard(3000);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "KHOI DONG ESP32-C3 DIEU KHIEN 4x HT16D35A LED BAR");
    ESP_LOGI(TAG, "==================================================");
    init_gpio();
    xTaskCreate(led_test_task, "led_test_task", 4096, NULL, 5, NULL);
}
