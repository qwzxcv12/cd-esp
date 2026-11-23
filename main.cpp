/**
 * ============================================================================
 * FIRMWARE TỰ ĐỘNG NHẬN DIỆN & HIỂN THỊ MA TRẬN LED 16x16 (ESP32-C3)
 * 4x HOLTEK HT16D35A + IC MỞ RỘNG PI4IOE5V6416 (TQFN-24)
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

// --- CHÂN TÍN HIỆU SPI CHO HT16D35A ---
#define PIN_CLK         GPIO_NUM_1
#define PIN_DIO         GPIO_NUM_0

// --- ĐÈN LED BÁO HOẠT ĐỘNG TRÊN BO ESP32-C3 ---
#define PIN_LED_BOARD   GPIO_NUM_8

#define I2C_PORT_NUM    I2C_NUM_0
#define I2C_FREQ_HZ     50000        // 50 kHz chống nhiễu tối đa

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

// In log trực tiếp qua ROM FIFO, tuyệt đối KHÔNG chặn luồng
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
    LOG_DIRECT("[HT16D35A] IC %d (CS%d) khoi tao OK!\r\n", ic_idx + 1, ic_idx + 1);
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

    // Thử 0x20
    if (probe_i2c_addr(0x20)) {
        s_i2c_addr = 0x20;
        s_i2c_ready = true;
        LOG_DIRECT("[I2C] >>> TIM THAY PI4IOE5V6416 TAI 0x20 (SDA=%d, SCL=%d) <<<\r\n", sda, scl);
        return true;
    }
    // Thử 0x21
    if (probe_i2c_addr(0x21)) {
        s_i2c_addr = 0x21;
        s_i2c_ready = true;
        LOG_DIRECT("[I2C] >>> TIM THAY PI4IOE5V6416 TAI 0x21 (SDA=%d, SCL=%d) <<<\r\n", sda, scl);
        return true;
    }

    // Thử quét các địa chỉ khác
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

    LOG_DIRECT("[I2C] Chua thay PI4IOE5V6416 phan hoi. Kiem tra nguon 3.3V va chan 24 RESET.\r\n");
    return;

init_chip:
    s_port0_state = 0xFF;
    i2c_write_byte(0x02, s_port0_state); // Output Port 0 = 0xFF
    i2c_write_byte(0x03, 0xFF);         // Output Port 1 = 0xFF
    i2c_write_byte(0x06, 0x00);         // Config Port 0: OUTPUT
    i2c_write_byte(0x07, 0x00);         // Config Port 1: OUTPUT
    i2c_write_byte(0x4F, 0x00);         // Push-Pull
    LOG_DIRECT("[I2C] Cau hinh PI4IOE5V6416 Port 0 thanh cong!\r\n");
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

static const uint16_t HEART_BIG[16] = {
    0x0000, 0x0E70, 0x3FFC, 0x7FFE, 0x7FFE, 0x7FFE, 0x3FFC, 0x3FFC,
    0x1FF8, 0x0FF0, 0x07E0, 0x03C0, 0x0180, 0x0000, 0x0000, 0x0000
};

static const uint16_t HEART_SMALL[16] = {
    0x0000, 0x0000, 0x0660, 0x0FF0, 0x0FF0, 0x0FF0, 0x07E0, 0x07E0,
    0x03C0, 0x0180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

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

static void anim_beating_heart(int duration_ms) {
    int elapsed = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        draw_bitmap(HEART_BIG, true, false, false);
        draw_pixel(4, 3, true, true, true);
        draw_pixel(4, 4, true, true, true);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(180));

        fb_clear();
        draw_bitmap(HEART_SMALL, true, false, false);
        draw_pixel(6, 4, true, true, true);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(140));

        fb_clear();
        draw_bitmap(HEART_BIG, true, false, false);
        draw_pixel(4, 3, true, true, true);
        draw_pixel(4, 4, true, true, true);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(200));

        fb_clear();
        draw_bitmap(HEART_SMALL, true, false, false);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(450));

        elapsed += (180 + 140 + 200 + 450);
    }
}

// TASK HIỂN THỊ
static void display_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(500));
    scan_and_init_i2c();

    if (s_i2c_ready) {
        LOG_DIRECT("=== KHOI TAO 4 CHIP HT16D35A ===\r\n");
        for (int i = 0; i < NUM_ICS; i++) {
            init_ht16d35a(i);
            vTaskDelay(pdMS_TO_TICKS(30));
        }
    }

    uint32_t count = 0;
    while (1) {
        if (!s_i2c_ready) {
            LOG_DIRECT("[Quet I2C lan %lu] Dang tim chip PI4IOE5V6416...\r\n", (unsigned long)count);
            scan_and_init_i2c();
            if (s_i2c_ready) {
                for (int i = 0; i < NUM_ICS; i++) {
                    init_ht16d35a(i);
                    vTaskDelay(pdMS_TO_TICKS(30));
                }
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
        } else {
            anim_beating_heart(4000);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        count++;
    }
}

// TASK NHẤP NHÁY ĐÈN BO MẠCH (GPIO 8)
static void blink_task(void *pvParameters) {
    gpio_reset_pin(PIN_LED_BOARD);
    gpio_set_direction(PIN_LED_BOARD, GPIO_MODE_OUTPUT);

    int level = 0;
    while (1) {
        gpio_set_level(PIN_LED_BOARD, level);
        level = !level;
        vTaskDelay(pdMS_TO_TICKS(250)); // Nhấp nháy 2 lần/giây
    }
}

extern "C" void app_main(void) {
    // Đặt stdout sang Non-Blocking để không bao giờ bị nghẽn
    int flags = fcntl(fileno(stdout), F_GETFL, 0);
    fcntl(fileno(stdout), F_SETFL, flags | O_NONBLOCK);

    vTaskDelay(pdMS_TO_TICKS(1000));

    LOG_DIRECT("\r\n\r\n=== FIRMWARE MA TRAN 16x16 DA KHOI DONG! ===\r\n");

    // 1. Cấu hình chân SPI HT16D35A
    gpio_reset_pin(PIN_CLK);
    gpio_reset_pin(PIN_DIO);
    gpio_set_direction(PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_CLK, 0);
    gpio_set_level(PIN_DIO, 0);

    // 2. Chạy task nhấp nháy đèn LED bo mạch (GPIO 8)
    xTaskCreate(blink_task, "blink_task", 2048, NULL, 3, NULL);

    // 3. Chạy task quét I2C & hiển thị
    xTaskCreate(display_task, "display_task", 4096, NULL, 5, NULL);
}
