/**
 * ============================================================================
 * DỰ ÁN: ĐIỀU KHIỂN MA TRẬN LED 4x HT16D35A BẰNG ESP32-C3
 * Thư mục: D:\thien\01_DuAn_Code\espc3 - led
 * Framework: ESP-IDF v5.1+ (Hỗ trợ build tự động qua cloud_builder.py)
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI ESP32-C3 -> MẠCH 4x HT16D35A:
 * - GND          -> GND ESP32-C3 & Mass nguồn 5V
 * - VDD (Logic)  -> 3.3V ESP32-C3 (hoặc 5V ngoài nếu nối chung)
 * - LED_VDD      -> Dương nguồn 5V ngoài (Chân 14, 21 trên IC)
 * - CLK          -> GPIO 1 (Chân 10 trên IC, nối chung cả 4 IC)
 * - DIO          -> GPIO 0 (Chân 9 trên IC, nối chung cả 4 IC)
 * - CS1          -> GPIO 20 (Chân 11 của IC 1 - Master)
 * - CS2          -> GPIO 21 (Chân 11 của IC 2 - Slave 1)
 * - CS3          -> GPIO 9 (Chân 11 của IC 3 - Slave 2)
 * - CS4          -> GPIO 10 (Chân 11 của IC 4 - Slave 3)
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

#define PIN_CS1         GPIO_NUM_20  // Chip Select IC 1 (Master)
#define PIN_CS2         GPIO_NUM_21  // Chip Select IC 2 (Slave 1)
#define PIN_CS3         GPIO_NUM_9   // Chip Select IC 3 (Slave 2)
#define PIN_CS4         GPIO_NUM_10  // Chip Select IC 4 (Slave 3)

#define NUM_ICS         4
static const gpio_num_t CS_PINS[NUM_ICS] = {PIN_CS1, PIN_CS2, PIN_CS3, PIN_CS4};

// --- BẢNG MÃ LỆNH CHUẨN THỰC TẾ HT16D35A (Holtek / Pimoroni) ---
#define CMD_SOFT_RESET       0xCC // Lệnh Reset mềm thực tế của HT16D35A
#define CMD_SOFT_RESET_ALT   0x00 // Lệnh Reset mềm phụ
#define CMD_GLOBAL_BRIGHTNESS 0x37 // Cài đặt độ sáng PWM (0x3F = 63 max PWM)
#define CMD_SCROLL_CTRL      0x20 // Điều khiển cuộn (0x00 = tắt cuộn)
#define CMD_COM_PIN_CTRL     0x41 // BẬT CỔNG COM (0xFF = mở COM0..COM7)
#define CMD_ROW_PIN_CTRL     0x42 // BẬT CỔNG ROW (4 bytes 0xFF = mở ROW0..ROW27)
#define CMD_SYSTEM_CTRL      0x35 // Bật Osc và Display (0x03 = Osc ON + Display ON)
#define CMD_WRITE_DDRAM      0x80 // Ghi dữ liệu vào RAM hiển thị (DDRAM)

// Hàm trễ microsecond dùng ROM function cho độ chính xác cao
static inline void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

// Hàm gửi 1 byte theo giao thức 3-wire (MSB first, chốt ở cạnh lên CLK, CLK nghỉ ở mức LOW)
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

// Gửi lệnh đơn (1 byte)
static void send_cmd_1b(uint8_t ic_idx, uint8_t cmd) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(5);
    write_byte(cmd);
    delay_us(5);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(5);
}

// Gửi lệnh 2 bytes (Mã lệnh + 1 tham số)
static void send_cmd_2b(uint8_t ic_idx, uint8_t cmd, uint8_t p1) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(5);
    write_byte(cmd);
    write_byte(p1);
    delay_us(5);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(5);
}

// Gửi lệnh 5 bytes (Mã lệnh + 4 tham số, dùng cho ROW PIN CTRL 0x42)
static void send_cmd_5b(uint8_t ic_idx, uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(5);
    write_byte(cmd);
    write_byte(p1);
    write_byte(p2);
    write_byte(p3);
    write_byte(p4);
    delay_us(5);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(5);
}

#define NUM_ROWS        28
#define NUM_COLS        8
#define BYTES_PER_IC    224

// Bộ đệm hiển thị đồ họa toàn cục cho 4 IC
static uint8_t fb[NUM_ICS][BYTES_PER_IC];

// Xóa sạch Framebuffer
static void fb_clear(void) {
    memset(fb, 0, sizeof(fb));
}

// Điền giá trị cho toàn bộ Framebuffer
static void fb_fill(uint8_t val) {
    memset(fb, val, sizeof(fb));
}

// Đặt độ sáng cho 1 điểm: row (0..27), col (0..7)
static void fb_set_pixel(uint8_t ic, uint8_t row, uint8_t col, uint8_t val) {
    if (ic >= NUM_ICS || row >= NUM_ROWS || col >= NUM_COLS) return;
    fb[ic][row * NUM_COLS + col] = val;
}

// Gửi Framebuffer xuống cả 4 IC HT16D35A
static void fb_render(void) {
    for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
        gpio_set_level(CS_PINS[ic], 0);
        delay_us(3);
        write_byte(CMD_WRITE_DDRAM);
        write_byte(0x00);
        for (int i = 0; i < BYTES_PER_IC; i++) {
            write_byte(fb[ic][i]);
        }
        delay_us(3);
        gpio_set_level(CS_PINS[ic], 1);
        delay_us(3);
    }
}

// Cài đặt độ sáng toàn cục PWM (0..63) cho cả 4 IC
static void set_global_brightness(uint8_t level) {
    if (level > 63) level = 63;
    for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
        send_cmd_2b(ic, CMD_GLOBAL_BRIGHTNESS, level);
    }
}

// Ghi toàn bộ Display Data RAM của một IC (224 vị trí: 28 ROW x 8 COM)
static void fill_display(uint8_t ic_idx, uint8_t pattern) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(5);
    
    write_byte(CMD_WRITE_DDRAM); // Lệnh ghi RAM (0x80)
    write_byte(0x00);            // Bắt đầu từ địa chỉ 0x00

    for (int i = 0; i < BYTES_PER_IC; i++) {
        write_byte(pattern);
    }

    delay_us(5);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(5);
}

// Khởi tạo một IC HT16D35A
static void init_ht16d35a(uint8_t ic_idx) {
    // 1. Reset mềm IC
    send_cmd_1b(ic_idx, CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));
    send_cmd_1b(ic_idx, CMD_SOFT_RESET_ALT);
    vTaskDelay(pdMS_TO_TICKS(10));

    // 2. Cài đặt độ sáng PWM tối đa (0x37 -> 0x3F)
    send_cmd_2b(ic_idx, CMD_GLOBAL_BRIGHTNESS, 0x3F);

    // 3. Tắt chức năng cuộn màn hình (0x20 -> 0x00)
    send_cmd_2b(ic_idx, CMD_SCROLL_CTRL, 0x00);

    // 4. Tạm tắt System Control để nạp cấu hình cổng (0x35 -> 0x00)
    send_cmd_2b(ic_idx, CMD_SYSTEM_CTRL, 0x00);

    // 5. Nạp dữ liệu ban đầu vào toàn bộ DDRAM
    fill_display(ic_idx, 0x00);

    // 6. [CỰC KỲ QUAN TRỌNG] BẬT TOÀN BỘ CỔNG COM: 0x41 -> 0xFF (COM0..COM7)
    send_cmd_2b(ic_idx, CMD_COM_PIN_CTRL, 0xFF);

    // 7. [CỰC KỲ QUAN TRỌNG] BẬT TOÀN BỘ CỔNG ROW: 0x42 -> 0xFF, 0xFF, 0xFF, 0xFF (ROW0..ROW27)
    send_cmd_5b(ic_idx, CMD_ROW_PIN_CTRL, 0xFF, 0xFF, 0xFF, 0xFF);

    // 8. BẬT BỘ DAO ĐỘNG HỆ THỐNG VÀ BẬT HIỂN THỊ (0x35 -> 0x03)
    send_cmd_2b(ic_idx, CMD_SYSTEM_CTRL, 0x03);

    ESP_LOGI(TAG, "IC %d: Khoi tao thanh cong voi COM PIN CTRL (0x41) va ROW PIN CTRL (0x42)", ic_idx + 1);
}

// Cấu hình các chân GPIO của ESP32-C3
static void init_gpio(void) {
    gpio_reset_pin(PIN_CLK);
    gpio_reset_pin(PIN_DIO);
    gpio_set_direction(PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_CLK, 0); // CLK nghỉ ở mức LOW theo chuẩn SPI Mode 0
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
static const uint8_t FONT_8X8[][8] = {
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

    // Ký tự đặc biệt: space, dash, colon (index 36, 37, 38)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Space
    {0x00, 0x00, 0x00, 0x7E, 0x7E, 0x00, 0x00, 0x00}, // -
    {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00}  // :
};

static const uint8_t* get_font_char(char c) {
    if (c >= '0' && c <= '9') return FONT_8X8[c - '0'];
    if (c >= 'A' && c <= 'Z') return FONT_8X8[10 + (c - 'A')];
    if (c >= 'a' && c <= 'z') return FONT_8X8[10 + (c - 'a')];
    if (c == '-') return FONT_8X8[37];
    if (c == ':') return FONT_8X8[38];
    return FONT_8X8[36]; // Space
}

// Vẽ 1 ký tự TO RÕ RÀNG phóng to 3x chiếm trọn 1 IC (24 dòng LED x 8 cột)
static void draw_char_big(uint8_t ic, char c, uint8_t brightness) {
    if (ic >= NUM_ICS) return;
    const uint8_t *glyph = get_font_char(c);
    
    // Xóa sạch IC này trong framebuffer
    for (int i = 0; i < BYTES_PER_IC; i++) {
        fb[ic][i] = 0;
    }

    // Phóng to 3x theo chiều dọc (8 dòng font -> 24 dòng LED từ dòng 2 đến 25)
    for (int r = 0; r < 8; r++) {
        uint8_t row_bits = glyph[r];
        int base_row = 2 + r * 3;
        for (int c_idx = 0; c_idx < 8; c_idx++) {
            if (row_bits & (1 << (7 - c_idx))) {
                fb_set_pixel(ic, base_row,     c_idx, brightness);
                fb_set_pixel(ic, base_row + 1, c_idx, brightness);
                fb_set_pixel(ic, base_row + 2, c_idx, brightness);
            }
        }
    }
}

// Hiển thị 4 ký tự TO cùng lúc trên 4 IC độc lập
static void display_text_4chars(const char *str) {
    fb_clear();
    for (int i = 0; i < NUM_ICS; i++) {
        if (str[i] != '\0') {
            draw_char_big(i, str[i], 0xFF);
        }
    }
    fb_render();
}

// ============================================================================
// CÁC HIỆU ỨNG HÌNH ẢNH (VISUAL ANIMATION EFFECTS)
// ============================================================================

// Hiệu ứng 1: Quét sóng ánh sáng tuần hoàn qua lại 4 IC
static void effect_wave_scan(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 1: SONG ANH SANG QUET (SCANNING WAVE)");
    int total_rows = NUM_ICS * NUM_ROWS; // 112 hàng
    int end_steps = duration_ms / 30;
    int head = 0;
    int dir = 1;
    for (int step = 0; step < end_steps; step++) {
        fb_clear();
        for (int t = 0; t < 6; t++) {
            int r = head - t * dir;
            if (r >= 0 && r < total_rows) {
                uint8_t ic = r / NUM_ROWS;
                uint8_t row = r % NUM_ROWS;
                uint8_t val = (t == 0) ? 0xFF : (t == 1 ? 0xB0 : (t == 2 ? 0x60 : (t == 3 ? 0x30 : 0x10)));
                for (uint8_t col = 0; col < NUM_COLS; col++) {
                    fb_set_pixel(ic, row, col, val);
                }
            }
        }
        fb_render();
        head += dir;
        if (head >= total_rows - 1) dir = -1;
        else if (head <= 0) dir = 1;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// Hiệu ứng 2: Nhịp thở ánh sáng êm dịu (Breathing / Pulse)
static void effect_breathing(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 2: NHIP THO (BREATHING PULSE)");
    fb_fill(0xFF);
    fb_render();
    int steps = duration_ms / 30;
    for (int step = 0; step < steps; step++) {
        float angle = (float)step * 0.1f;
        uint8_t bri = (uint8_t)(31.5f * (1.0f + sinf(angle)));
        set_global_brightness(bri);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    set_global_brightness(63); // Trả lại độ sáng cực đại
}

// Hiệu ứng 3: Mưa sao băng ma trận (Digital Matrix Rain)
static void effect_matrix_rain(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 3: MUA SAO BANG (DIGITAL MATRIX RAIN)");
    int total_cols = NUM_ICS * NUM_COLS; // 32 cột
    int drop_pos[32];
    int drop_speed[32];
    for (int i = 0; i < total_cols; i++) {
        drop_pos[i] = rand() % NUM_ROWS;
        drop_speed[i] = 1 + rand() % 2;
    }
    int steps = duration_ms / 40;
    for (int step = 0; step < steps; step++) {
        // Giảm mờ dần vết sáng cũ tạo đuôi rơi
        for (int ic = 0; ic < NUM_ICS; ic++) {
            for (int i = 0; i < BYTES_PER_IC; i++) {
                fb[ic][i] = (uint8_t)(fb[ic][i] * 0.65f);
            }
        }
        for (int c = 0; c < total_cols; c++) {
            uint8_t ic = c / NUM_COLS;
            uint8_t col = c % NUM_COLS;
            fb_set_pixel(ic, drop_pos[c], col, 0xFF);
            drop_pos[c] += drop_speed[c];
            if (drop_pos[c] >= NUM_ROWS) {
                drop_pos[c] = 0;
                drop_speed[c] = 1 + rand() % 2;
            }
        }
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

// Hiệu ứng 4: Cột sóng âm thanh nhảy nhót (Equalizer VU-Meter)
static void effect_equalizer(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 4: SONG NHAC (EQUALIZER VU-METER)");
    int total_cols = NUM_ICS * NUM_COLS; // 32 cột âm thanh
    float heights[32] = {0};
    float targets[32] = {0};
    int steps = duration_ms / 35;
    for (int step = 0; step < steps; step++) {
        fb_clear();
        if (step % 5 == 0) {
            for (int c = 0; c < total_cols; c++) {
                targets[c] = 2 + (rand() % (NUM_ROWS - 2));
            }
        }
        for (int c = 0; c < total_cols; c++) {
            heights[c] += (targets[c] - heights[c]) * 0.35f;
            int h = (int)heights[c];
            uint8_t ic = c / NUM_COLS;
            uint8_t col = c % NUM_COLS;
            for (int r = 0; r < h; r++) {
                uint8_t val = (r > h - 3) ? 0xFF : 0x70;
                fb_set_pixel(ic, r, col, val);
            }
            if (h < NUM_ROWS) {
                fb_set_pixel(ic, h, col, 0xFF); // Đỉnh cột sóng
            }
        }
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(35));
    }
}

// Hiệu ứng 5: Bầu trời sao lấp lánh (Twinkling Stars)
static void effect_twinkling_stars(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 5: BAU TROI SAO LAP LANH (TWINKLING STARS)");
    fb_clear();
    int steps = duration_ms / 35;
    for (int step = 0; step < steps; step++) {
        for (int ic = 0; ic < NUM_ICS; ic++) {
            for (int i = 0; i < BYTES_PER_IC; i++) {
                if (fb[ic][i] > 12) fb[ic][i] -= 12;
                else fb[ic][i] = 0;
            }
        }
        for (int k = 0; k < 3; k++) {
            uint8_t ic = rand() % NUM_ICS;
            uint8_t r = rand() % NUM_ROWS;
            uint8_t c = rand() % NUM_COLS;
            fb_set_pixel(ic, r, c, 180 + rand() % 75);
        }
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(35));
    }
}

// Hiệu ứng 6: Bàn cờ caro đan xen (Checkerboard)
static void effect_checkerboard(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 6: BAN CO CARO DAO PHA (CHECKERBOARD)");
    int steps = duration_ms / 250;
    for (int step = 0; step < steps; step++) {
        fb_clear();
        uint8_t phase = step % 2;
        for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
            for (uint8_t r = 0; r < NUM_ROWS; r++) {
                for (uint8_t c = 0; c < NUM_COLS; c++) {
                    if ((r + c + ic + phase) % 2 == 0) {
                        fb_set_pixel(ic, r, c, 0xFF);
                    }
                }
            }
        }
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

// ============================================================================
// CHƯƠNG TRÌNH TEST HIỂN THỊ CHỮ SỐ & CHỮ CÁI TO RÕ RÀNG
// ============================================================================

// 1. Đếm các số to từ 1 đến 9 trên cả 4 IC
static void test_digits_1_to_9_big(void) {
    ESP_LOGI(TAG, "=== HIEN THI CAC SO TU 1 DEN 9 (TO RO RANG TREN 4 IC) ===");
    for (char c = '1'; c <= '9'; c++) {
        ESP_LOGI(TAG, "-> So to: %c", c);
        char buf[5] = {c, c, c, c, '\0'};
        display_text_4chars(buf);
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
}

// 2. Hiển thị nhóm 4 số và 4 chữ cái (Mỗi IC hiển thị 1 ký tự to)
static void test_groups_4chars(void) {
    ESP_LOGI(TAG, "=== HIEN THI NHOM 4 SO VA CHU CAI TO ===");
    const char *groups[] = {
        "1234", "5678", "9012",
        "ABCD", "EFGH", "IJKL",
        "MNOP", "QRST", "UVWX", "YZ--"
    };
    int n = sizeof(groups) / sizeof(groups[0]);
    for (int i = 0; i < n; i++) {
        ESP_LOGI(TAG, "-> Nhom ky tu to: %s", groups[i]);
        display_text_4chars(groups[i]);
        vTaskDelay(pdMS_TO_TICKS(1800));
    }
}

// 3. Hiển thị từng chữ cái to từ A đến Z trên cả 4 IC
static void test_letters_A_to_Z(void) {
    ESP_LOGI(TAG, "=== HIEN THI CAC CHU CAI TU A DEN Z (TO RO RANG) ===");
    for (char c = 'A'; c <= 'Z'; c++) {
        ESP_LOGI(TAG, "-> Chu cai: %c", c);
        char buf[5] = {c, c, c, c, '\0'};
        display_text_4chars(buf);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

// Task chạy thử nghiệm kiểm tra mạch LED với các hiệu ứng và chữ số to
static void led_test_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== BAT DAU TEST MA TRAN LED: SO 1-9 & CHU CAI TO RO RANG ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    set_global_brightness(63);

    while (1) {
        test_digits_1_to_9_big();      // 1. Đếm số to 1 -> 9 trên màn hình
        vTaskDelay(pdMS_TO_TICKS(1000));

        test_groups_4chars();          // 2. Nhóm 4 số & chữ cái: 1234, 5678, ABCD, EFGH...
        vTaskDelay(pdMS_TO_TICKS(1000));

        test_letters_A_to_Z();         // 3. Từng chữ cái to A -> Z
        vTaskDelay(pdMS_TO_TICKS(1000));

        effect_wave_scan(4000);        // Đan xen hiệu ứng sóng ánh sáng 4s
        effect_matrix_rain(4000);      // Đan xen hiệu ứng mưa rơi 4s
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Khoi dong he thong ESP32-C3 HT16D35A LED Driver");
    init_gpio();
    xTaskCreate(led_test_task, "led_test_task", 4096, NULL, 5, NULL);
}
