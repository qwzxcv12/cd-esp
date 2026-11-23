/**
 * ============================================================================
 * HIỂN THỊ CHỮ SỐ (0-9) VÀ CHỮ CÁI (A-Z) TO RÕ RÀNG TRỌN VẸN 16x16
 * ĐỔI MÀU FULL RGB TRÊN MA TRẬN 16x16 (4x HOLTEK HT16D35A)
 * ============================================================================
 * SƠ ĐỒ ĐẤU NỐI ESP32-C3:
 * - CLK : GPIO 1  (chung 4 IC)
 * - DIO : GPIO 0  (chung 4 IC)
 * - CS1 : GPIO 20 (IC1: Cột 0..3)
 * - CS2 : GPIO 21 (IC2: Cột 4..7)
 * - CS3 : GPIO 9  (IC3: Cột 8..11)
 * - CS4 : GPIO 10 (IC4: Cột 12..15)
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
// Font 16x16 to ro rang cho 0-9 va A-Z (Arial Bold)
static const uint16_t FONT_16X16[36][16] = {
    { 0x07C0, 0x0FE0, 0x0EE0, 0x1C70, 0x1C70, 0x1C70, 0x1C70, 0x1C70, 0x1C70, 0x1C70, 0x1C70, 0x1C70, 0x0EE0, 0x0FE0, 0x07C0, 0x0000 }, // [ 0] '0'
    { 0x01C0, 0x01C0, 0x03C0, 0x0FC0, 0x1FC0, 0x1DC0, 0x11C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x0000 }, // [ 1] '1'
    { 0x07C0, 0x0FE0, 0x1C70, 0x1C70, 0x0070, 0x0070, 0x00E0, 0x01E0, 0x03C0, 0x0380, 0x0700, 0x0F00, 0x0E00, 0x1FF0, 0x1FF0, 0x0000 }, // [ 2] '2'
    { 0x07C0, 0x0FE0, 0x1C70, 0x1C70, 0x0070, 0x00F0, 0x03E0, 0x03E0, 0x00F0, 0x0070, 0x0070, 0x1C70, 0x1C70, 0x0FE0, 0x07C0, 0x0000 }, // [ 3] '3'
    { 0x00E0, 0x01E0, 0x01E0, 0x03E0, 0x03E0, 0x06E0, 0x0CE0, 0x0CE0, 0x18E0, 0x30E0, 0x3FF8, 0x3FF8, 0x00E0, 0x00E0, 0x00E0, 0x0000 }, // [ 4] '4'
    { 0x07E0, 0x07E0, 0x0E00, 0x0E00, 0x0E00, 0x0FC0, 0x1FE0, 0x1CF0, 0x0070, 0x0070, 0x0070, 0x1C70, 0x1EE0, 0x0FE0, 0x07C0, 0x0000 }, // [ 5] '5'
    { 0x03C0, 0x0FE0, 0x0EF0, 0x1C60, 0x1C00, 0x1DC0, 0x1FE0, 0x1EF0, 0x1C70, 0x1C70, 0x1C70, 0x1C70, 0x0EE0, 0x0FE0, 0x0380, 0x0000 }, // [ 6] '6'
    { 0x1FF0, 0x1FF0, 0x0070, 0x00E0, 0x01C0, 0x01C0, 0x0380, 0x0380, 0x0700, 0x0700, 0x0700, 0x0E00, 0x0E00, 0x0E00, 0x0E00, 0x0000 }, // [ 7] '7'
    { 0x07C0, 0x0FE0, 0x1C70, 0x1C70, 0x1C70, 0x0EE0, 0x07C0, 0x07C0, 0x0EE0, 0x1C70, 0x1C70, 0x1C70, 0x1EF0, 0x0FE0, 0x07C0, 0x0000 }, // [ 8] '8'
    { 0x0380, 0x0FE0, 0x0EE0, 0x1C70, 0x1C70, 0x1C70, 0x1C70, 0x1EF0, 0x0FF0, 0x0770, 0x0070, 0x0C70, 0x1EE0, 0x0FE0, 0x0780, 0x0000 }, // [ 9] '9'
    { 0x0F80, 0x0F80, 0x0F80, 0x1DC0, 0x1DC0, 0x1DC0, 0x18E0, 0x38E0, 0x38E0, 0x3FE0, 0x7FF0, 0x7FF0, 0x7070, 0xE038, 0xE038, 0x0000 }, // [10] 'A'
    { 0x3FE0, 0x3FF0, 0x3FF8, 0x3838, 0x3838, 0x3838, 0x3FF0, 0x3FF0, 0x3FF8, 0x381C, 0x381C, 0x381C, 0x3FFC, 0x3FF8, 0x3FF0, 0x0000 }, // [11] 'B'
    { 0x03E0, 0x0FF8, 0x1FF8, 0x1E3C, 0x3C18, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3C18, 0x1C3C, 0x1FF8, 0x0FF8, 0x03E0, 0x0000 }, // [12] 'C'
    { 0x3FE0, 0x3FF0, 0x3FF8, 0x3838, 0x383C, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x3838, 0x3FF8, 0x3FF0, 0x3FC0, 0x0000 }, // [13] 'D'
    { 0x3FF8, 0x3FF8, 0x3FF8, 0x3800, 0x3800, 0x3800, 0x3FF0, 0x3FF0, 0x3FF0, 0x3800, 0x3800, 0x3800, 0x3FF8, 0x3FF8, 0x3FF8, 0x0000 }, // [14] 'E'
    { 0x1FF8, 0x1FF8, 0x1FF8, 0x1C00, 0x1C00, 0x1C00, 0x1FF0, 0x1FF0, 0x1FF0, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x0000 }, // [15] 'F'
    { 0x03E0, 0x0FF8, 0x1FFC, 0x3C1E, 0x780C, 0x7000, 0x7000, 0x70FE, 0x70FE, 0x70FE, 0x380E, 0x3C1E, 0x1FFE, 0x0FFC, 0x03E0, 0x0000 }, // [16] 'G'
    { 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x3FFC, 0x3FFC, 0x3FFC, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x0000 }, // [17] 'H'
    { 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0000 }, // [18] 'I'
    { 0x0070, 0x0070, 0x0070, 0x0070, 0x0070, 0x0070, 0x0070, 0x0070, 0x0070, 0x3870, 0x3870, 0x3C70, 0x1FF0, 0x1FE0, 0x0780, 0x0000 }, // [19] 'J'
    { 0x7078, 0x70F0, 0x71E0, 0x71C0, 0x7380, 0x7780, 0x7F80, 0x7FC0, 0x7DC0, 0x78E0, 0x70E0, 0x7070, 0x7070, 0x7038, 0x7038, 0x0000 }, // [20] 'K'
    { 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1FF8, 0x1FF8, 0x1FF8, 0x0000 }, // [21] 'L'
    { 0x0000, 0x7C1F, 0x7C1F, 0x7E3F, 0x7637, 0x7637, 0x7637, 0x7777, 0x7367, 0x7367, 0x7367, 0x73E7, 0x71C7, 0x71C7, 0x71C7, 0x0000 }, // [22] 'M'
    { 0x381C, 0x3C1C, 0x3E1C, 0x3E1C, 0x3F1C, 0x3B1C, 0x3B9C, 0x399C, 0x39DC, 0x38DC, 0x38FC, 0x387C, 0x387C, 0x383C, 0x381C, 0x0000 }, // [23] 'N'
    { 0x07E0, 0x1FF8, 0x3FFC, 0x3C3C, 0x781E, 0x700E, 0x700E, 0x700E, 0x700E, 0x700E, 0x781E, 0x3C3C, 0x3FFC, 0x1FF8, 0x07E0, 0x0000 }, // [24] 'O'
    { 0x3FE0, 0x3FF0, 0x3FF8, 0x3838, 0x3838, 0x3838, 0x3FF8, 0x3FF0, 0x3FE0, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x0000 }, // [25] 'P'
    { 0x07C0, 0x1FF0, 0x3FF8, 0x3C78, 0x783C, 0x701C, 0x701C, 0x701C, 0x709C, 0x79DC, 0x3CF8, 0x3FF8, 0x1FFC, 0x07DE, 0x0004, 0x0000 }, // [26] 'Q'
    { 0x7FC0, 0x7FE0, 0x7FF0, 0x7070, 0x7070, 0x7070, 0x7FF0, 0x7FE0, 0x7F80, 0x73C0, 0x71E0, 0x70E0, 0x70F0, 0x7070, 0x7078, 0x0000 }, // [27] 'R'
    { 0x07C0, 0x1FF0, 0x1FF0, 0x3C78, 0x3838, 0x3C00, 0x1F80, 0x0FF0, 0x03F0, 0x0078, 0x3838, 0x3C78, 0x1FF8, 0x1FF0, 0x07C0, 0x0000 }, // [28] 'S'
    { 0x1FFC, 0x1FFC, 0x1FFC, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x01C0, 0x0000 }, // [29] 'T'
    { 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x381C, 0x3C3C, 0x1FF8, 0x1FF8, 0x07E0, 0x0000 }, // [30] 'U'
    { 0x701C, 0x701C, 0x3838, 0x3838, 0x3838, 0x1C70, 0x1C70, 0x1C70, 0x0EE0, 0x0EE0, 0x0EE0, 0x07C0, 0x07C0, 0x07C0, 0x0380, 0x0000 }, // [31] 'V'
    { 0x0000, 0x0000, 0xC386, 0xE386, 0x638C, 0x66CC, 0x66CC, 0x36D8, 0x36D8, 0x36D8, 0x1C78, 0x1C70, 0x1C70, 0x1C70, 0x0000, 0x0000 }, // [32] 'W'
    { 0x3838, 0x3838, 0x1C70, 0x0EE0, 0x0EE0, 0x07C0, 0x07C0, 0x0380, 0x07C0, 0x07C0, 0x0EE0, 0x0EE0, 0x1C70, 0x3838, 0x3838, 0x0000 }, // [33] 'X'
    { 0x701C, 0x701C, 0x3838, 0x1C70, 0x1C70, 0x0EE0, 0x07C0, 0x07C0, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0380, 0x0000 }, // [34] 'Y'
    { 0x1FF8, 0x1FF8, 0x1FF8, 0x0078, 0x00F0, 0x01E0, 0x01C0, 0x0380, 0x0700, 0x0F00, 0x1E00, 0x3C00, 0x3FF8, 0x3FF8, 0x3FF8, 0x0000 }, // [35] 'Z'
};


static const char *TAG = "MATRIX_16X16";

// --- CHÂN GPIO ESP32-C3 ---
#define PIN_CLK         GPIO_NUM_1
#define PIN_DIO         GPIO_NUM_0

#define PIN_CS1         GPIO_NUM_20  // IC 1
#define PIN_CS2         GPIO_NUM_21  // IC 2
#define PIN_CS3         GPIO_NUM_9   // IC 3
#define PIN_CS4         GPIO_NUM_10  // IC 4

#define NUM_ICS         4
static const gpio_num_t CS_PINS[NUM_ICS] = {PIN_CS1, PIN_CS2, PIN_CS3, PIN_CS4};

// --- MÃ LỆNH HOLTEK HT16D35A ---
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
    gpio_set_level(CS_PINS[ic_idx], 0); delay_us(4);
    write_byte(cmd);
    delay_us(4); gpio_set_level(CS_PINS[ic_idx], 1); delay_us(4);
}

static void send_cmd_2b(uint8_t ic_idx, uint8_t cmd, uint8_t p1) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0); delay_us(4);
    write_byte(cmd); write_byte(p1);
    delay_us(4); gpio_set_level(CS_PINS[ic_idx], 1); delay_us(4);
}

static void send_cmd_5b(uint8_t ic_idx, uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0); delay_us(4);
    write_byte(cmd); write_byte(p1); write_byte(p2); write_byte(p3); write_byte(p4);
    delay_us(4); gpio_set_level(CS_PINS[ic_idx], 1); delay_us(4);
}

static void fb_clear(void) {
    memset(fb, 0, sizeof(fb));
}

static void fb_render(void) {
    for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
        gpio_set_level(CS_PINS[ic], 0); delay_us(4);
        write_byte(CMD_WRITE_DDRAM); write_byte(0x00);
        for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
            write_byte(fb[ic][r]);
        }
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
    ESP_LOGI(TAG, "IC %d (CS GPIO %d) khoi tao OK", ic_idx + 1, CS_PINS[ic_idx]);
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
// HỆ THỐNG ĐỒ HỌA MA TRẬN 16x16
// ============================================================================

/**
 * Đặt màu cho 1 pixel theo tọa độ ma trận 16x16:
 * @param x: Cột 0..15 (0 bên trái, 15 bên phải)
 * @param y: Hàng 0..15 (0 ở đỉnh trên cùng, 15 ở đáy dưới cùng)
 * @param r, g, b: Bật/tắt các màu Đỏ, Xanh lá, Xanh dương
 */
static void draw_pixel(int x, int y, bool r, bool g, bool b) {
    if (x < 0 || x >= 16 || y < 0 || y >= 16) return;

    uint8_t ic = x / 4;
    uint8_t col = x % 4;
    uint8_t row = 15 - y; // Đỉnh là y=0 (row 15), Đáy là y=15 (row 0)

    uint8_t t = row % 8;
    uint8_t addr_R = t * 3 + 0;
    uint8_t addr_G = t * 3 + 1;
    uint8_t addr_B = t * 3 + 2;

    uint8_t com_bit = (row >= 8) ? ((3 - col) * 2) : ((3 - col) * 2 + 1);
    uint8_t mask = (1 << com_bit);

    if (r) fb[ic][addr_R] |= mask; else fb[ic][addr_R] &= ~mask;
    if (g) fb[ic][addr_G] |= mask; else fb[ic][addr_G] &= ~mask;
    if (b) fb[ic][addr_B] |= mask; else fb[ic][addr_B] &= ~mask;
}

// Bảng màu RGB rực rỡ
typedef struct {
    bool r;
    bool g;
    bool b;
    const char *name;
} ColorRGB;

static const ColorRGB PALETTE[] = {
    {true,  false, false, "DO (RED)"},
    {false, true,  false, "XANH LA (GREEN)"},
    {false, false, true,  "XANH DUONG (BLUE)"},
    {true,  true,  false, "VANG (YELLOW)"},
    {false, true,  true,  "CYAN (XANH LO)"},
    {true,  false, true,  "MAGENTA (TIM)"},
    {true,  true,  true,  "TRANG (WHITE)"},
};
#define NUM_COLORS (sizeof(PALETTE) / sizeof(PALETTE[0]))

/**
 * Hiển thị 1 glyph 16x16 với màu RGB chỉ định
 */
static void show_glyph(const uint16_t glyph[16], ColorRGB color) {
    fb_clear();
    for (int y = 0; y < 16; y++) {
        uint16_t row_bits = glyph[y];
        for (int x = 0; x < 16; x++) {
            if (row_bits & (1 << (15 - x))) {
                draw_pixel(x, y, color.r, color.g, color.b);
            }
        }
    }
    fb_render();
}

/**
 * Hiển thị 1 ký tự ('0'..'9' hoặc 'A'..'Z')
 */
static void show_char(char c, ColorRGB color) {
    int idx = -1;
    if (c >= '0' && c <= '9') {
        idx = c - '0';
    } else if (c >= 'A' && c <= 'Z') {
        idx = 10 + (c - 'A');
    } else if (c >= 'a' && c <= 'z') {
        idx = 10 + (c - 'a');
    }

    if (idx >= 0 && idx < 36) {
        show_glyph(FONT_16X16[idx], color);
    }
}

// ============================================================================
// TASK ĐIỀU KHIỂN CHÍNH
// ============================================================================
static void display_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== KHOI DONG MAN HINH 16x16 FULL RGB ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    int color_idx = 0;

    while (1) {
        // ====================================================================
        // PHẦN 1: HIỂN THỊ CÁC CHỮ SỐ TỪ 0 ĐẾN 9 (MỖI SỐ ĐỔI MÀU RGB)
        // ====================================================================
        ESP_LOGI(TAG, ">>> BAT DAU HIEN THI CAC SO TU 0 DEN 9 <<<");
        for (char num = '0'; num <= '9'; num++) {
            ColorRGB col = PALETTE[color_idx % NUM_COLORS];
            color_idx++;

            ESP_LOGI(TAG, "Hien thi So: %c | Mau: %s", num, col.name);
            show_char(num, col);

            // Giữ 1.2 giây để quan sát rõ ràng
            vTaskDelay(pdMS_TO_TICKS(1200));
        }

        // Chớp tắt nghỉ 0.5s
        fb_clear(); fb_render();
        vTaskDelay(pdMS_TO_TICKS(500));

        // ====================================================================
        // PHẦN 2: HIỂN THỊ CÁC CHỮ CÁI TỪ A ĐẾN Z (MỖI CHỮ ĐỔI MÀU RGB)
        // ====================================================================
        ESP_LOGI(TAG, ">>> BAT DAU HIEN THI CAC CHU CAI TU A DEN Z <<<");
        for (char ch = 'A'; ch <= 'Z'; ch++) {
            ColorRGB col = PALETTE[color_idx % NUM_COLORS];
            color_idx++;

            ESP_LOGI(TAG, "Hien thi Chu: %c | Mau: %s", ch, col.name);
            show_char(ch, col);

            // Giữ 1.0 giây mỗi chữ cái
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // Chớp tắt nghỉ 0.5s
        fb_clear(); fb_render();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "ESP32-C3 16x16 RGB DISPLAY - NUMBERS & LETTERS");
    init_gpio();
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
}
