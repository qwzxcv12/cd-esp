/**
 * ============================================================================
 * TRÌNH DIỄN ĐỒ HỌA TOÀN DIỆN MA TRẬN 16x16 FULL RGB (ESP32-C3)
 * 4x HOLTEK HT16D35A + IC MỞ RỘNG PI4IOE5V6416 (TQFN-24)
 * ============================================================================
 * BAO GỒM ĐẦY ĐỦ CÁC BÀI TEST & HIỆU ỨNG ĐỈNH CAO:
 * 1. TEST MÀU TOÀN MÀN HÌNH: Đỏ -> Xanh lá -> Xanh dương -> Trắng
 * 2. CHẠY CHỮ SỐ 0 -> 9 ĐA SẮC MÀU (Font 16x16 sắc nét)
 * 3. CHẠY CHỮ CÁI TEST: E -> S -> P -> 3 -> 2 -> O -> K
 * 4. 6 HOẠT HÌNH PIXEL ART:
 *    - Trái tim đập (Beating Heart)
 *    - Mặt cười nháy mắt (Winking Smiley)
 *    - Pac-Man há mồm ăn kẹo (Pacman Retro)
 *    - Mây mưa rơi & Sấm chớp (Rain & Thunder)
 *    - Cột sóng âm nhạc (Audio Equalizer)
 *    - Ngọn lửa bập bùng (Doom Fire Effect)
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "font16x16.h"

// --- CẤU HÌNH CHÂN SPI CHO HT16D35A ---
#define PIN_CLK         GPIO_NUM_1
#define PIN_DIO         GPIO_NUM_0

// --- ĐÈN LED BO MẠCH ESP32-C3 ---
#define PIN_LED_BOARD   GPIO_NUM_8

#define I2C_PORT_NUM    I2C_NUM_0
#define I2C_FREQ_HZ     50000        // 50 kHz tốc độ chuẩn, chống nhiễu đường dài

#define NUM_ICS         4
#define NUM_ROWS_PER_IC 28

// CS1 -> Bit 3, CS2 -> Bit 2, CS3 -> Bit 1, CS4 -> Bit 0
static const uint8_t CS_BITS[NUM_ICS] = {3, 2, 1, 0};

static uint8_t s_i2c_addr = 0x20;
static bool s_i2c_ready = false;
static uint8_t s_port0_state = 0xFF;

static uint8_t fb[NUM_ICS][NUM_ROWS_PER_IC];

static inline void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

#define LOG_DIRECT(fmt, ...) do { \
    esp_rom_printf(fmt, ##__VA_ARGS__); \
} while(0)

static esp_err_t i2c_write_byte(uint8_t reg, uint8_t val) {
    if (!s_i2c_ready) return ESP_FAIL;
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(I2C_PORT_NUM, s_i2c_addr, buf, 2, pdMS_TO_TICKS(15));
}

static inline void cs_select(uint8_t ic_idx) {
    if (ic_idx >= NUM_ICS || !s_i2c_ready) return;
    s_port0_state &= ~(1 << CS_BITS[ic_idx]);
    i2c_write_byte(0x02, s_port0_state);
    delay_us(2);
}

static inline void cs_deselect(uint8_t ic_idx) {
    if (ic_idx >= NUM_ICS || !s_i2c_ready) return;
    s_port0_state |= (1 << CS_BITS[ic_idx]);
    i2c_write_byte(0x02, s_port0_state);
    delay_us(2);
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
    cs_select(ic_idx);
    write_byte(cmd);
    cs_deselect(ic_idx);
}

static void send_cmd_2b(uint8_t ic_idx, uint8_t cmd, uint8_t p1) {
    if (ic_idx >= NUM_ICS) return;
    cs_select(ic_idx);
    write_byte(cmd);
    write_byte(p1);
    cs_deselect(ic_idx);
}

static void send_cmd_5b(uint8_t ic_idx, uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
    if (ic_idx >= NUM_ICS) return;
    cs_select(ic_idx);
    write_byte(cmd);
    write_byte(p1);
    write_byte(p2);
    write_byte(p3);
    write_byte(p4);
    cs_deselect(ic_idx);
}

static void fb_clear(void) {
    memset(fb, 0, sizeof(fb));
}

static void fb_render(void) {
    if (!s_i2c_ready) return;
    for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
        cs_select(ic);
        write_byte(0x80); // CMD_WRITE_DDRAM
        write_byte(0x00);
        for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
            write_byte(fb[ic][r]);
        }
        cs_deselect(ic);
    }
}

static void init_ht16d35a(uint8_t ic_idx) {
    send_cmd_1b(ic_idx, 0xCC); // Soft reset
    delay_us(100);
    send_cmd_1b(ic_idx, 0x00);
    vTaskDelay(pdMS_TO_TICKS(10));

    send_cmd_2b(ic_idx, 0x31, 0x01); // Binary mode
    send_cmd_2b(ic_idx, 0x32, 0x07); // 8 COM
    send_cmd_2b(ic_idx, 0x34, 0x01); // Cascade master
    send_cmd_2b(ic_idx, 0x35, 0x00);
    send_cmd_2b(ic_idx, 0x36, 0x07); // Current ratio max
    send_cmd_2b(ic_idx, 0x37, 0x3F); // Global brightness max
    send_cmd_2b(ic_idx, 0x38, 0x00);
    send_cmd_2b(ic_idx, 0x20, 0x00);
    send_cmd_2b(ic_idx, 0x41, 0xFF);
    send_cmd_5b(ic_idx, 0x42, 0xFF, 0xFF, 0xFF, 0xFF);

    cs_select(ic_idx);
    write_byte(0x80);
    write_byte(0x00);
    for (int r = 0; r < NUM_ROWS_PER_IC; r++) write_byte(0x00);
    cs_deselect(ic_idx);

    send_cmd_2b(ic_idx, 0x35, 0x03); // System on + Display on
    LOG_DIRECT("[HT16D35A] IC %d khoi tao OK!\r\n", ic_idx + 1);
}

static bool probe_i2c_addr(uint8_t addr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(20));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

static bool try_i2c_pair(gpio_num_t sda, gpio_num_t scl) {
    i2c_driver_delete(I2C_PORT_NUM);
    vTaskDelay(pdMS_TO_TICKS(10));

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.scl_io_num = scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FREQ_HZ;

    if (i2c_param_config(I2C_PORT_NUM, &conf) != ESP_OK) return false;
    if (i2c_driver_install(I2C_PORT_NUM, conf.mode, 0, 0, 0) != ESP_OK) return false;

    if (probe_i2c_addr(0x20)) {
        s_i2c_addr = 0x20;
        s_i2c_ready = true;
        LOG_DIRECT("[I2C] >>> KET NOI THANH CONG PI4IOE5V6416 TAI 0x20 (SDA=%d, SCL=%d) <<<\r\n", sda, scl);
        return true;
    }
    if (probe_i2c_addr(0x21)) {
        s_i2c_addr = 0x21;
        s_i2c_ready = true;
        LOG_DIRECT("[I2C] >>> KET NOI THANH CONG PI4IOE5V6416 TAI 0x21 (SDA=%d, SCL=%d) <<<\r\n", sda, scl);
        return true;
    }
    for (uint8_t a = 0x10; a < 0x78; a++) {
        if (a == 0x20 || a == 0x21) continue;
        if (probe_i2c_addr(a)) {
            s_i2c_addr = a;
            s_i2c_ready = true;
            LOG_DIRECT("[I2C] >>> PHAT HIEN THIET BI TAI 0x%02X (SDA=%d, SCL=%d) <<<\r\n", a, sda, scl);
            return true;
        }
    }
    return false;
}

static void scan_and_init_i2c(void) {
    // Chiều 1: SDA=20, SCL=21
    if (try_i2c_pair(GPIO_NUM_20, GPIO_NUM_21)) goto init_chip;
    // Chiều 2: SDA=21, SCL=20
    if (try_i2c_pair(GPIO_NUM_21, GPIO_NUM_20)) goto init_chip;
    // Chiều 3: SDA=4, SCL=5
    if (try_i2c_pair(GPIO_NUM_4, GPIO_NUM_5)) goto init_chip;

    LOG_DIRECT("[I2C] Dang cho ket noi chip PI4IOE5V6416...\r\n");
    return;

init_chip:
    s_port0_state = 0xFF;
    i2c_write_byte(0x02, s_port0_state); // Output Port 0 = 0xFF (CS1..CS4 HIGH)
    i2c_write_byte(0x03, 0xFF);         // Output Port 1 = 0xFF
    i2c_write_byte(0x06, 0x00);         // Config Port 0: OUTPUT
    i2c_write_byte(0x07, 0x00);         // Config Port 1: OUTPUT
    i2c_write_byte(0x4F, 0x00);         // Push-Pull output
    LOG_DIRECT("[I2C] Cau hinh PI4IOE5V6416 Port 0 lam CS1..CS4 thanh cong!\r\n");
}

// ============================================================================
// HỆ THỐNG ĐỒ HỌA MA TRẬN 16x16 FULL RGB
// ============================================================================
static void draw_pixel(int x, int y, bool r, bool g, bool b) {
    if (x < 0 || x >= 16 || y < 0 || y >= 16) return;

    uint8_t ic = x / 4;
    uint8_t col = x % 4;
    uint8_t row = 15 - y;

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

static void draw_bitmap(const uint16_t bmp[16], bool r, bool g, bool b) {
    for (int y = 0; y < 16; y++) {
        uint16_t bits = bmp[y];
        for (int x = 0; x < 16; x++) {
            if (bits & (1 << (15 - x))) {
                draw_pixel(x, y, r, g, b);
            }
        }
    }
}

// Vẽ ký tự Font 16x16 (0-9, A-Z)
static void draw_char(char c, bool r, bool g, bool b) {
    int idx = -1;
    if (c >= '0' && c <= '9') idx = c - '0';
    else if (c >= 'A' && c <= 'Z') idx = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z') idx = c - 'a' + 10;

    if (idx >= 0 && idx < 36) {
        draw_bitmap(FONT_16X16[idx], r, g, b);
    }
}

// ----------------------------------------------------------------------------
// 1. TEST MÀU TOÀN MÀN HÌNH (R, G, B, WHITE)
// ----------------------------------------------------------------------------
static void test_full_colors(void) {
    LOG_DIRECT(">>> 1. TEST TOAN MAN HINH: RED -> GREEN -> BLUE -> WHITE <<<\r\n");

    // ĐỎ
    fb_clear();
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) draw_pixel(x, y, true, false, false);
    fb_render(); vTaskDelay(pdMS_TO_TICKS(1000));

    // XANH LÁ
    fb_clear();
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) draw_pixel(x, y, false, true, false);
    fb_render(); vTaskDelay(pdMS_TO_TICKS(1000));

    // XANH DƯƠNG
    fb_clear();
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) draw_pixel(x, y, false, false, true);
    fb_render(); vTaskDelay(pdMS_TO_TICKS(1000));

    // TRẮNG
    fb_clear();
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) draw_pixel(x, y, true, true, true);
    fb_render(); vTaskDelay(pdMS_TO_TICKS(1000));
}

// ----------------------------------------------------------------------------
// 2. CHẠY CHỮ SỐ 0 -> 9 (ĐA SẮC MÀU)
// ----------------------------------------------------------------------------
static void test_digits_0_to_9(void) {
    LOG_DIRECT(">>> 2. TEST CHU SO 0 -> 9 <<<\r\n");
    bool colors[10][3] = {
        {true, false, false}, // 0: Đỏ
        {false, true, false}, // 1: Xanh lá
        {false, false, true}, // 2: Xanh dương
        {true, true, false},  // 3: Vàng
        {false, true, true},  // 4: Xanh ngọc Cyan
        {true, false, true},  // 5: Hồng Magenta
        {true, true, true},   // 6: Trắng
        {true, true, false},  // 7: Vàng
        {false, true, true},  // 8: Xanh ngọc
        {true, false, true}   // 9: Hồng
    };

    for (int d = 0; d <= 9; d++) {
        fb_clear();
        draw_char('0' + d, colors[d][0], colors[d][1], colors[d][2]);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(650));
    }
}

// ----------------------------------------------------------------------------
// 3. CHẠY TỪ KHÓA CHỮ CÁI: E -> S -> P -> 3 -> 2 -> O -> K
// ----------------------------------------------------------------------------
static void test_words_esp_ok(void) {
    LOG_DIRECT(">>> 3. TEST CHU CAI: E-S-P-3-2  O-K <<<\r\n");
    const char text[] = "ESP32OK";
    for (int i = 0; text[i] != '\0'; i++) {
        fb_clear();
        draw_char(text[i], true, true, false); // Màu vàng rực rỡ
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

// ----------------------------------------------------------------------------
// 4. BỘ 6 HOẠT HÌNH PIXEL ART
// ----------------------------------------------------------------------------
static const uint16_t HEART_BIG[16] = {
    0x0000, 0x0E70, 0x3FFC, 0x7FFE, 0x7FFE, 0x7FFE, 0x3FFC, 0x3FFC,
    0x1FF8, 0x0FF0, 0x07E0, 0x03C0, 0x0180, 0x0000, 0x0000, 0x0000
};
static const uint16_t HEART_SMALL[16] = {
    0x0000, 0x0000, 0x0660, 0x0FF0, 0x0FF0, 0x0FF0, 0x07E0, 0x07E0,
    0x03C0, 0x0180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static void anim_beating_heart(int duration_ms) {
    LOG_DIRECT("  [Animation] 1. Trai tim dap...\r\n");
    int elapsed = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        draw_bitmap(HEART_BIG, true, false, false);
        draw_pixel(4, 3, true, true, true);
        draw_pixel(4, 4, true, true, true);
        fb_render(); vTaskDelay(pdMS_TO_TICKS(180));

        fb_clear();
        draw_bitmap(HEART_SMALL, true, false, false);
        draw_pixel(6, 4, true, true, true);
        fb_render(); vTaskDelay(pdMS_TO_TICKS(140));

        fb_clear();
        draw_bitmap(HEART_BIG, true, false, false);
        draw_pixel(4, 3, true, true, true);
        draw_pixel(4, 4, true, true, true);
        fb_render(); vTaskDelay(pdMS_TO_TICKS(200));

        fb_clear();
        draw_bitmap(HEART_SMALL, true, false, false);
        fb_render(); vTaskDelay(pdMS_TO_TICKS(450));

        elapsed += (180 + 140 + 200 + 450);
    }
}

static const uint16_t SMILEY_OPEN[16] = {
    0x07E0, 0x1FF8, 0x3FFC, 0x73CE, 0x73CE, 0xFFFF, 0xFFFF, 0xFFFF,
    0xE007, 0xF00F, 0x7C3E, 0x7FFE, 0x3FFC, 0x1FF8, 0x07E0, 0x0000
};
static const uint16_t SMILEY_WINK[16] = {
    0x07E0, 0x1FF8, 0x3FFC, 0x73FE, 0x7386, 0xFFFF, 0xFFFF, 0xFFFF,
    0xE007, 0xF00F, 0x7C3E, 0x7FFE, 0x3FFC, 0x1FF8, 0x07E0, 0x0000
};

static void anim_winking_smiley(int duration_ms) {
    LOG_DIRECT("  [Animation] 2. Mat cuoi nhay mat...\r\n");
    int elapsed = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        draw_bitmap(SMILEY_OPEN, true, true, false);
        fb_render(); vTaskDelay(pdMS_TO_TICKS(800));

        fb_clear();
        draw_bitmap(SMILEY_WINK, true, true, false);
        fb_render(); vTaskDelay(pdMS_TO_TICKS(250));

        elapsed += 1050;
    }
}

static const uint16_t PACMAN_OPEN[16] = {
    0x03C0, 0x0FF0, 0x1FF8, 0x3FFC, 0x7FE0, 0x7F80, 0x7E00, 0x7C00,
    0x7C00, 0x7E00, 0x7F80, 0x7FE0, 0x3FFC, 0x1FF8, 0x0FF0, 0x03C0
};
static const uint16_t PACMAN_CLOSED[16] = {
    0x03C0, 0x0FF0, 0x1FF8, 0x3FFC, 0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE,
    0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE, 0x3FFC, 0x1FF8, 0x0FF0, 0x03C0
};

static void anim_pacman(int duration_ms) {
    LOG_DIRECT("  [Animation] 3. Pac-Man an keo...\r\n");
    int elapsed = 0, step = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        if (step % 2 == 0) draw_bitmap(PACMAN_OPEN, true, true, false);
        else draw_bitmap(PACMAN_CLOSED, true, true, false);

        draw_pixel(13, 7, false, true, true);
        draw_pixel(14, 7, false, true, true);
        draw_pixel(13, 8, false, true, true);
        draw_pixel(14, 8, false, true, true);

        fb_render(); vTaskDelay(pdMS_TO_TICKS(180));
        elapsed += 180; step++;
    }
}

static const uint16_t CLOUD_BMP[16] = {
    0x0000, 0x0000, 0x0780, 0x1FE0, 0x3FF8, 0x7FFC, 0xFFFF, 0xFFFF,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static void anim_rain_and_thunder(int duration_ms) {
    LOG_DIRECT("  [Animation] 4. May mua & Sam chop...\r\n");
    int elapsed = 0, drop_frame = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        draw_bitmap(CLOUD_BMP, false, true, true);

        for (int x = 2; x < 15; x += 3) {
            int y = 8 + ((drop_frame + x) % 8);
            draw_pixel(x, y, false, false, true);
        }

        if (drop_frame % 14 == 0) {
            draw_pixel(7, 8, true, true, false);
            draw_pixel(8, 9, true, true, false);
            draw_pixel(7, 10, true, true, false);
            draw_pixel(6, 11, true, true, false);
            draw_pixel(7, 12, true, true, false);
            draw_pixel(8, 13, true, true, false);
        }

        fb_render(); vTaskDelay(pdMS_TO_TICKS(120));
        elapsed += 120; drop_frame++;
    }
}

static void anim_equalizer(int duration_ms) {
    LOG_DIRECT("  [Animation] 5. Cot song Equalizer...\r\n");
    int elapsed = 0;
    int peaks[16] = {0};
    while (elapsed < duration_ms) {
        fb_clear();
        for (int x = 0; x < 16; x++) {
            int height = 2 + (rand() % 14);
            if (height > peaks[x]) peaks[x] = height;
            else if (peaks[x] > 0) peaks[x]--;

            for (int y = 15; y >= (15 - height); y--) {
                if (y >= 11) draw_pixel(x, y, false, true, false);      // Xanh lá
                else if (y >= 5) draw_pixel(x, y, true, true, false);   // Vàng
                else draw_pixel(x, y, true, false, false);              // Đỏ đỉnh
            }
            if (peaks[x] > 0) {
                draw_pixel(x, 15 - peaks[x], true, true, true);
            }
        }
        fb_render(); vTaskDelay(pdMS_TO_TICKS(90));
        elapsed += 90;
    }
}

static void anim_doom_fire(int duration_ms) {
    LOG_DIRECT("  [Animation] 6. Ngon lua Doom Fire...\r\n");
    int elapsed = 0;
    uint8_t fire[16][16] = {0};

    while (elapsed < duration_ms) {
        fb_clear();
        for (int x = 0; x < 16; x++) fire[x][15] = (rand() % 2 == 0) ? 3 : 2;

        for (int y = 0; y < 15; y++) {
            for (int x = 0; x < 16; x++) {
                int decay = rand() % 2;
                int src_x = (x + decay) % 16;
                int val = fire[src_x][y + 1];
                fire[x][y] = (val > decay) ? (val - decay) : 0;
            }
        }

        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                uint8_t intensity = fire[x][y];
                if (intensity == 3) draw_pixel(x, y, true, true, false);
                else if (intensity == 2) draw_pixel(x, y, true, false, false);
                else if (intensity == 1) draw_pixel(x, y, true, false, false);
            }
        }

        fb_render(); vTaskDelay(pdMS_TO_TICKS(80));
        elapsed += 80;
    }
}

// ============================================================================
// TASK ĐIỀU KHIỂN CHÍNH
// ============================================================================
static void display_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(500));
    scan_and_init_i2c();

    if (s_i2c_ready) {
        LOG_DIRECT("=== KHOI TAO 4 CHIP HT16D35A QUA PI4IOE5V6416 ===\r\n");
        for (int i = 0; i < NUM_ICS; i++) {
            init_ht16d35a(i);
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        LOG_DIRECT("=== KHOI TAO THANH CONG! BAT DAU TRINH DIEN ===\r\n");
    }

    uint32_t round = 0;
    while (1) {
        if (!s_i2c_ready) {
            LOG_DIRECT("[Quet I2C] Dang tim chip PI4IOE5V6416...\r\n");
            scan_and_init_i2c();
            if (s_i2c_ready) {
                for (int i = 0; i < NUM_ICS; i++) {
                    init_ht16d35a(i);
                    vTaskDelay(pdMS_TO_TICKS(30));
                }
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
        } else {
            // 1. Test màu toàn màn hình (R, G, B, White)
            test_full_colors();
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            // 2. Chữ số 0 -> 9
            test_digits_0_to_9();
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            // 3. Chữ cái ESP32 OK
            test_words_esp_ok();
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            // 4. Trái tim đập (6s)
            anim_beating_heart(6000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            // 5. Mặt cười nháy mắt (5s)
            anim_winking_smiley(5000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            // 6. Pac-Man (6s)
            anim_pacman(6000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            // 7. Mây mưa sấm chớp (6s)
            anim_rain_and_thunder(6000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            // 8. Cột sóng nhạc Equalizer (7s)
            anim_equalizer(7000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            // 9. Ngọn lửa bập bùng (7s)
            anim_doom_fire(7000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(500));

            round++;
            LOG_DIRECT(">>> Hoan thanh vong lap trinh dien thu: %lu <<<\r\n", (unsigned long)round);
        }
    }
}

// Task nhấp nháy đèn LED bo mạch (GPIO 8)
static void blink_task(void *pvParameters) {
    gpio_reset_pin(PIN_LED_BOARD);
    gpio_set_direction(PIN_LED_BOARD, GPIO_MODE_OUTPUT);

    int level = 0;
    while (1) {
        gpio_set_level(PIN_LED_BOARD, level);
        level = !level;
        vTaskDelay(pdMS_TO_TICKS(250)); // Nhấp nháy 2 lần mỗi giây
    }
}

extern "C" void app_main(void) {
    // Đặt stdout sang Non-blocking
    int flags = fcntl(fileno(stdout), F_GETFL, 0);
    fcntl(fileno(stdout), F_SETFL, flags | O_NONBLOCK);

    vTaskDelay(pdMS_TO_TICKS(1000));

    LOG_DIRECT("\r\n\r\n======================================================\r\n");
    LOG_DIRECT("  FIRMWARE MA TRAN LED 16x16 RGB + PI4IOE5V6416\r\n");
    LOG_DIRECT("  CLK=GPIO 1 | DIO=GPIO 0 | SDA=GPIO 20 | SCL=GPIO 21\r\n");
    LOG_DIRECT("======================================================\r\n\r\n");

    // 1. Cấu hình chân SPI HT16D35A
    gpio_reset_pin(PIN_CLK);
    gpio_reset_pin(PIN_DIO);
    gpio_set_direction(PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_CLK, 0);
    gpio_set_level(PIN_DIO, 0);

    // 2. Chạy task nhấp nháy đèn bo mạch
    xTaskCreate(blink_task, "blink_task", 2048, NULL, 3, NULL);

    // 3. Chạy task điều khiển hiển thị
    xTaskCreate(display_task, "display_task", 6144, NULL, 5, NULL);
}
