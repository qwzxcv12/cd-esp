/**
 * ============================================================================
 * TRÌNH DIỄN ĐỒ HỌA MA TRẬN 16x16 FULL RGB + ÂM THANH RETRO ES8388 (ESP32-C3)
 * - MÀN HÌNH: 4x HOLTEK HT16D35A + PI4IOE5V6416 (CLK=1, DIO=0, SDA=20, SCL=21)
 * - ÂM THANH: EVEREST SEMI ES8388 DAC (MCLK=8, BCLK=5, DOUT=6, WS=7, I2C=20/21)
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "font16x16.h"

// --- CẤU HÌNH CHÂN SPI CHO HT16D35A ---
#define PIN_CLK         GPIO_NUM_1
#define PIN_DIO         GPIO_NUM_0

// --- CẤU HÌNH I2S CHO ES8388 DAC ---
#define PIN_I2S_MCLK    GPIO_NUM_8
#define PIN_I2S_BCLK    GPIO_NUM_5
#define PIN_I2S_DOUT    GPIO_NUM_6
#define PIN_I2S_WS      GPIO_NUM_7
#define I2S_SAMPLE_RATE 44100

// --- I2C BUS CHUNG (PI4IOE5V6416 & ES8388) ---
#define I2C_PORT_NUM    I2C_NUM_0
#define I2C_FREQ_HZ     50000

#define NUM_ICS         4
#define NUM_ROWS_PER_IC 28

// CS1 -> Bit 3, CS2 -> Bit 2, CS3 -> Bit 1, CS4 -> Bit 0
static const uint8_t CS_BITS[NUM_ICS] = {3, 2, 1, 0};

static uint8_t s_i2c_addr = 0x20;
static bool s_i2c_ready = false;
static uint8_t s_port0_state = 0xFF;

static uint8_t s_es8388_addr = 0x10;
static bool s_es8388_ready = false;
static bool s_es8388_inited = false;
static gpio_num_t s_mclk_pin = PIN_I2S_MCLK;

static i2s_chan_handle_t s_tx_handle = NULL;
static uint8_t fb[NUM_ICS][NUM_ROWS_PER_IC];

#define LOG_DIRECT(fmt, ...) do { \
    esp_rom_printf(fmt, ##__VA_ARGS__); \
} while(0)

static inline void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

// Bảng Sin 256 mẫu chuẩn 16-bit
static const int16_t SINE_TABLE[256] = {
    0, 804, 1607, 2410, 3211, 4011, 4807, 5601, 6392, 7179, 7961, 8739, 9511, 10278, 11038, 11792,
    12539, 13278, 14009, 14732, 15446, 16150, 16845, 17530, 18204, 18867, 19519, 20159, 20787, 21402, 22004, 22594,
    23169, 23731, 24278, 24811, 25329, 25831, 26318, 26789, 27244, 27683, 28105, 28510, 28897, 29268, 29621, 29955,
    30272, 30571, 30851, 31113, 31356, 31580, 31785, 31970, 32137, 32284, 32412, 32520, 32609, 32678, 32727, 32757,
    32767, 32757, 32727, 32678, 32609, 32520, 32412, 32284, 32137, 31970, 31785, 31580, 31356, 31113, 30851, 30571,
    30272, 29955, 29621, 29268, 28897, 28510, 28105, 27683, 27244, 26789, 26318, 25831, 25329, 24811, 24278, 23731,
    23169, 22594, 22004, 21402, 20787, 20159, 19519, 18867, 18204, 17530, 16845, 16150, 15446, 14732, 14009, 13278,
    12539, 11792, 11038, 10278, 9511, 8739, 7961, 7179, 6392, 5601, 4807, 4011, 3211, 2410, 1607, 804,
    0, -804, -1607, -2410, -3211, -4011, -4807, -5601, -6392, -7179, -7961, -8739, -9511, -10278, -11038, -11792,
    -12539, -13278, -14009, -14732, -15446, -16150, -16845, -17530, -18204, -18867, -19519, -20159, -20787, -21402, -22004, -22594,
    -23169, -23731, -24278, -24811, -25329, -25831, -26318, -26789, -27244, -27683, -28105, -28510, -28897, -29268, -29621, -29955,
    -30272, -30571, -30851, -31113, -31356, -31580, -31785, -31970, -32137, -32284, -32412, -32520, -32609, -32678, -32727, -32757,
    -32767, -32757, -32727, -32678, -32609, -32520, -32412, -32284, -32137, -31970, -31785, -31580, -31356, -31113, -30851, -30571,
    -30272, -29955, -29621, -29268, -28897, -28510, -28105, -27683, -27244, -26789, -26318, -25831, -25329, -24811, -24278, -23731,
    -23169, -22594, -22004, -21402, -20787, -20159, -19519, -18867, -18204, -17530, -16845, -16150, -15446, -14732, -14009, -13278,
    -12539, -11792, -11038, -10278, -9511, -8739, -7961, -7179, -6392, -5601, -4807, -4011, -3211, -2410, -1607, -804
};

// ============================================================================
// GIAO TIẾP I2C: PI4IOE5V6416 & ES8388
// ============================================================================
static esp_err_t i2c_write_byte(uint8_t reg, uint8_t val) {
    if (!s_i2c_ready) return ESP_FAIL;
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(I2C_PORT_NUM, s_i2c_addr, buf, 2, pdMS_TO_TICKS(15));
}

static esp_err_t es8388_write_reg(uint8_t reg, uint8_t val) {
    if (!s_es8388_ready) return ESP_FAIL;
    uint8_t buf[2] = {reg, val};
    esp_err_t err = i2c_master_write_to_device(I2C_PORT_NUM, s_es8388_addr, buf, 2, pdMS_TO_TICKS(20));
    if (err != ESP_OK) {
        LOG_DIRECT("[ES8388] Loi ghi thanh ghi 0x%02X=0x%02X (%s)\r\n", reg, val, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t es8388_read_reg(uint8_t reg, uint8_t *val) {
    if (!s_es8388_ready || !val) return ESP_FAIL;
    return i2c_master_write_read_device(I2C_PORT_NUM, s_es8388_addr, &reg, 1, val, 1, pdMS_TO_TICKS(20));
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
    send_cmd_2b(ic_idx, 0x36, 0x03); // Current ratio an toan, tranh sut ap
    send_cmd_2b(ic_idx, 0x37, 0x18); // Global brightness ro net, em nguon
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

    // 1. Kiểm tra nhanh chip PI4IOE5V6416 (0x20 hoặc 0x21)
    if (probe_i2c_addr(0x20)) {
        s_i2c_addr = 0x20;
        s_i2c_ready = true;
        LOG_DIRECT("[I2C] >>> PI4IOE5V6416 san sang tai 0x20 (SDA=%d, SCL=%d) <<<\r\n", sda, scl);
    } else if (probe_i2c_addr(0x21)) {
        s_i2c_addr = 0x21;
        s_i2c_ready = true;
        LOG_DIRECT("[I2C] >>> PI4IOE5V6416 san sang tai 0x21 (SDA=%d, SCL=%d) <<<\r\n", sda, scl);
    }

    // 2. Kiểm tra nhanh chip DAC ES8388 (0x10 hoặc 0x11)
    if (probe_i2c_addr(0x10)) {
        s_es8388_addr = 0x10;
        s_es8388_ready = true;
        LOG_DIRECT("[I2C] >>> ES8388 DAC san sang tai 0x10 <<<\r\n");
    } else if (probe_i2c_addr(0x11)) {
        s_es8388_addr = 0x11;
        s_es8388_ready = true;
        LOG_DIRECT("[I2C] >>> ES8388 DAC san sang tai 0x11 <<<\r\n");
    }

    if (s_i2c_ready) return true;

    // Nếu chưa thấy, quét nhanh các địa chỉ còn lại
    for (uint8_t a = 0x10; a < 0x78; a++) {
        if (a == 0x20 || a == 0x21) continue;
        if (probe_i2c_addr(a)) {
            LOG_DIRECT("[I2C] Phat hien thiet bi tai 0x%02X\r\n", a);
            if (a == 0x10 || a == 0x11) {
                s_es8388_addr = a;
                s_es8388_ready = true;
            }
        }
    }
    return s_i2c_ready;
}

static void i2c_bus_recover(gpio_num_t sda, gpio_num_t scl) {
    gpio_reset_pin(sda);
    gpio_reset_pin(scl);
    gpio_set_direction(sda, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_direction(scl, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(sda, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(scl, GPIO_PULLUP_ONLY);
    gpio_set_level(sda, 1);
    gpio_set_level(scl, 1);
    delay_us(10);
    for (int i = 0; i < 9; i++) {
        if (gpio_get_level(sda) == 1) break;
        gpio_set_level(scl, 0);
        delay_us(10);
        gpio_set_level(scl, 1);
        delay_us(10);
    }
    gpio_set_level(sda, 0);
    delay_us(10);
    gpio_set_level(scl, 1);
    delay_us(10);
    gpio_set_level(sda, 1);
    delay_us(10);
}

static void scan_and_init_i2c(void) {
    i2c_bus_recover(GPIO_NUM_20, GPIO_NUM_21);
    if (try_i2c_pair(GPIO_NUM_20, GPIO_NUM_21)) goto init_chip;
    if (try_i2c_pair(GPIO_NUM_21, GPIO_NUM_20)) goto init_chip;
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
// BỘ ĐIỀU KHIỂN ÂM THANH ES8388 & I2S MASTER
// ============================================================================
static bool init_i2s(void) {
    LOG_DIRECT("[I2S] Dang khoi tao I2S Master (MCLK=%d, BCLK=%d, DOUT=%d, WS=%d, Rate=%dHz)...\r\n",
               s_mclk_pin, PIN_I2S_BCLK, PIN_I2S_DOUT, PIN_I2S_WS, I2S_SAMPLE_RATE);

    // Kiem tra nhanh tinh trang cham chan (PU/PD + keo xuong 0 o dong thap 5mA trong 20us)
    const gpio_num_t i2s_pins[4] = {s_mclk_pin, PIN_I2S_BCLK, PIN_I2S_DOUT, PIN_I2S_WS};
    const char *i2s_names[4] = {"MCLK", "BCLK", "DOUT", "WS"};
    for (int i = 0; i < 4; i++) {
        gpio_reset_pin(i2s_pins[i]);
        gpio_set_direction(i2s_pins[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(i2s_pins[i], GPIO_PULLUP_ONLY);
        delay_us(30);
        int hi = gpio_get_level(i2s_pins[i]);
        gpio_set_pull_mode(i2s_pins[i], GPIO_PULLDOWN_ONLY);
        delay_us(30);
        int lo = gpio_get_level(i2s_pins[i]);
        gpio_set_pull_mode(i2s_pins[i], GPIO_FLOATING);
        gpio_set_direction(i2s_pins[i], GPIO_MODE_INPUT_OUTPUT);
        gpio_set_drive_capability(i2s_pins[i], GPIO_DRIVE_CAP_0);
        gpio_set_level(i2s_pins[i], 0);
        delay_us(20);
        int drv0 = gpio_get_level(i2s_pins[i]);
        gpio_set_drive_capability(i2s_pins[i], GPIO_DRIVE_CAP_2);
        gpio_reset_pin(i2s_pins[i]);
        const char *status = "(OK)";
        if (drv0 == 1)      status = "<-- CHAM CUNG 3V3!";
        else if (hi == 0)   status = "<-- CHAM CUNG GND!";
        else if (lo == 1)   status = "(Co tro keo len 3V3, van lai duoc 0)";
        LOG_DIRECT("[I2S PinCheck] %s(GPIO%d): PU=%d PD=%d Drv0=%d %s\r\n",
                   i2s_names[i], i2s_pins[i], hi, lo, drv0, status);
    }

    // Kiem tra thuc te xem ES8388 co nhan duoc MCLK(GPIO8) va tra ve SCLK(GPIO5)/LRCK(GPIO7) hay khong
    if (s_es8388_ready) {
        es8388_write_reg(0x02, 0xF3);
        es8388_write_reg(0x00, 0x16);
        es8388_write_reg(0x01, 0x00);
        es8388_write_reg(0x18, 0x02); // 256Fs
        es8388_write_reg(0x2B, 0x80);
        es8388_write_reg(0x08, 0x84); // Master Mode, BCLK = MCLK / 4
        es8388_write_reg(0x02, 0x00);

        gpio_reset_pin(PIN_I2S_BCLK);
        gpio_reset_pin(PIN_I2S_WS);
        gpio_set_direction(PIN_I2S_BCLK, GPIO_MODE_INPUT);
        gpio_set_direction(PIN_I2S_WS, GPIO_MODE_INPUT);
        gpio_set_pull_mode(PIN_I2S_BCLK, GPIO_PULLDOWN_ONLY);
        gpio_set_pull_mode(PIN_I2S_WS, GPIO_PULLDOWN_ONLY);

        ledc_timer_config_t tc = {};
        tc.speed_mode      = LEDC_LOW_SPEED_MODE;
        tc.duty_resolution = LEDC_TIMER_2_BIT;
        tc.timer_num       = LEDC_TIMER_0;
        tc.freq_hz         = 10000000; // 10 MHz MCLK (80M / 4 / 2 exact) -> SCLK=2.5MHz, LRCK=39kHz
        tc.clk_cfg         = LEDC_AUTO_CLK;
        esp_err_t terr = ledc_timer_config(&tc);

        ledc_channel_config_t cc = {};
        cc.gpio_num   = s_mclk_pin;
        cc.speed_mode = LEDC_LOW_SPEED_MODE;
        cc.channel    = LEDC_CHANNEL_0;
        cc.intr_type  = LEDC_INTR_DISABLE;
        cc.timer_sel  = LEDC_TIMER_0;
        cc.duty       = 2;
        cc.hpoint     = 0;
        esp_err_t cerr = ledc_channel_config(&cc);

        vTaskDelay(pdMS_TO_TICKS(10));
        int b_edges = 0, w_edges = 0;
        int lb = gpio_get_level(PIN_I2S_BCLK);
        int lw = gpio_get_level(PIN_I2S_WS);
        for (int i = 0; i < 30000; i++) {
            int b = gpio_get_level(PIN_I2S_BCLK);
            int w = gpio_get_level(PIN_I2S_WS);
            if (b != lb) { b_edges++; lb = b; }
            if (w != lw) { w_edges++; lw = w; }
        }
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        es8388_write_reg(0x08, 0x00); // Tra ve Slave Mode
        gpio_reset_pin(s_mclk_pin);
        gpio_reset_pin(PIN_I2S_BCLK);
        gpio_reset_pin(PIN_I2S_WS);
        LOG_DIRECT("[ES8388 Loopback @10MHz] terr=%s cerr=%s -> SCLK(GPIO5)=%d, LRCK(GPIO7)=%d\r\n",
                   esp_err_to_name(terr), esp_err_to_name(cerr), b_edges, w_edges);
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &s_tx_handle, NULL);
    if (ret != ESP_OK) {
        LOG_DIRECT("[I2S] Loi tao channel: %s\r\n", esp_err_to_name(ret));
        return false;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = s_mclk_pin,
            .bclk = PIN_I2S_BCLK,
            .ws   = PIN_I2S_WS,
            .dout = PIN_I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ret = i2s_channel_init_std_mode(s_tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        LOG_DIRECT("[I2S] Loi init std mode: %s\r\n", esp_err_to_name(ret));
        return false;
    }

    int16_t zeros[256] = {0};
    size_t preloaded = 0;
    esp_err_t pret = i2s_channel_preload_data(s_tx_handle, zeros, sizeof(zeros), &preloaded);
    LOG_DIRECT("[I2S] Preload DMA: %s (bytes=%u)\r\n", esp_err_to_name(pret), (unsigned)preloaded);

    ret = i2s_channel_enable(s_tx_handle);
    if (ret != ESP_OK) {
        LOG_DIRECT("[I2S] Loi enable channel: %s\r\n", esp_err_to_name(ret));
        return false;
    }

    // Bom du lieu im lang bang timeout_ms=1000ms (IDF v5.1 tinh bang ms, khong phai ticks)
    size_t written = 0;
    bool warmup_ok = true;
    for (int i = 0; i < 16; i++) {
        esp_err_t wret = i2s_channel_write(s_tx_handle, zeros, sizeof(zeros), &written, 1000);
        if (wret != ESP_OK) {
            LOG_DIRECT("[I2S] LOI warmup write #%d: %s (written=%u)\r\n",
                       i, esp_err_to_name(wret), (unsigned)written);
            warmup_ok = false;
            break;
        }
    }

    LOG_DIRECT("[I2S] Warmup write: %s | I2S Driver san sang!\r\n", warmup_ok ? "OK" : "FAILED");
    return warmup_ok;
}

static bool init_es8388(void) {
    if (!s_es8388_ready) {
        if (probe_i2c_addr(0x10)) {
            s_es8388_addr = 0x10;
            s_es8388_ready = true;
        } else if (probe_i2c_addr(0x11)) {
            s_es8388_addr = 0x11;
            s_es8388_ready = true;
        }
    }

    if (!s_es8388_ready) {
        LOG_DIRECT("[ES8388] CHUA PHAT HIEN ES8388 TAI 0x10 / 0x11 (Kiem tra nguon/han I2C)!\r\n");
        return false;
    }

    LOG_DIRECT("[ES8388] Khoi tao chip DAC tai dia chi 0x%02X...\r\n", s_es8388_addr);

    // 1. Mute & tat dau ra truoc khi nap dien tu VMID
    es8388_write_reg(0x19, 0x04); // Mute DAC
    es8388_write_reg(0x04, 0xC0); // Tam tat ca DAC & OUT1/OUT2
    es8388_write_reg(0x02, 0xF3); // Reset / stop ADC & DAC state machines
    es8388_write_reg(0x08, 0x00); // I2S Slave Mode (ESP32 cap MCLK, BCLK, LRCK)
    es8388_write_reg(0x2B, 0x80); // DACCONTROL21: slrck=1 (ADC & DAC dung chung LRCK)
    es8388_write_reg(0x2D, 0x00); // DACCONTROL23: VROI = 0

    // 2. Bat nguon tham chieu VMID / EnRef & Ibias (full normal analog bias)
    es8388_write_reg(0x00, 0x05); // EnRef=1, VMIDSEL=01 (50k nap nhanh tu VMID)
    es8388_write_reg(0x01, 0x00); // Power ON Analog, Vref & Ibias Generator (full bias)
    es8388_write_reg(0x03, 0xFF); // Power down ADC
    es8388_write_reg(0x07, 0x7C); // Analog voltage management default

    // 3. Format I2S 16-bit, MCLK = 256*Fs (Single Speed)
    es8388_write_reg(0x17, 0x18); // DACCONTROL1: 16-bit Philips I2S
    es8388_write_reg(0x18, 0x02); // DACCONTROL2: 256*Fs

    // 4. Dinh tuyen DAC -> Output Mixer
    es8388_write_reg(0x26, 0x00); // DACCONTROL16
    es8388_write_reg(0x27, 0x90); // DACCONTROL17: Left DAC -> Left Mixer (0dB)
    es8388_write_reg(0x2A, 0x90); // DACCONTROL20: Right DAC -> Right Mixer (0dB)

    // 5. Giam muc analog xuong -18dB (0x12) de khong lam vo tieng/clipping choi tai o tang PAM8403 (gain 24dB)
    es8388_write_reg(0x2E, 0x12); // LOUT1VOL (-18dB)
    es8388_write_reg(0x2F, 0x12); // ROUT1VOL (-18dB)
    es8388_write_reg(0x30, 0x12); // LOUT2VOL (-18dB)
    es8388_write_reg(0x31, 0x12); // ROUT2VOL (-18dB)
    es8388_write_reg(0x1A, 0x00); // Left Digital Volume = 0dB
    es8388_write_reg(0x1B, 0x00); // Right Digital Volume = 0dB

    // 6. Bat State Machine & cho VMID nap on dinh 150ms ROI MOI MO OUT1/OUT2 & Unmute
    es8388_write_reg(0x02, 0x00); // Power UP khoi DAC & DLL
    vTaskDelay(pdMS_TO_TICKS(150));
    es8388_write_reg(0x00, 0x16); // SameFs=1, EnRef=1, VMIDSEL=10 (500k normal mode)
    es8388_write_reg(0x04, 0x3C); // DAC L/R ON + OUT1/OUT2 ON
    es8388_write_reg(0x19, 0x00); // Unmute DAC!

    // 7. Doc nguoc thanh ghi de xac minh ES8388
    uint8_t r00 = 0, r01 = 0, r02 = 0, r04 = 0, r17 = 0, r18 = 0, r19 = 0, r27 = 0, r2b = 0, r2e = 0, r30 = 0;
    es8388_read_reg(0x00, &r00);
    es8388_read_reg(0x01, &r01);
    es8388_read_reg(0x02, &r02);
    es8388_read_reg(0x04, &r04);
    es8388_read_reg(0x17, &r17);
    es8388_read_reg(0x18, &r18);
    es8388_read_reg(0x19, &r19);
    es8388_read_reg(0x27, &r27);
    es8388_read_reg(0x2B, &r2b);
    es8388_read_reg(0x2E, &r2e);
    es8388_read_reg(0x30, &r30);
    LOG_DIRECT("[ES8388] Doc lai: R00=0x%02X R01=0x%02X R02=0x%02X R04=0x%02X R17=0x%02X R18=0x%02X R19=0x%02X R27=0x%02X R2B=0x%02X OUT1=0x%02X OUT2=0x%02X\r\n",
               r00, r01, r02, r04, r17, r18, r19, r27, r2b, r2e, r30);

    s_es8388_inited = true;
    LOG_DIRECT("[ES8388] Khoi tao thanh cong!\r\n");
    return true;
}

static void check_es8388_health(const char *tag) {
    if (!s_es8388_ready) return;
    uint8_t r00 = 0, r02 = 0, r04 = 0, r19 = 0;
    esp_err_t e0 = es8388_read_reg(0x00, &r00);
    esp_err_t e1 = es8388_read_reg(0x04, &r04);
    es8388_read_reg(0x02, &r02);
    es8388_read_reg(0x19, &r19);
    if (e0 != ESP_OK || e1 != ESP_OK) {
        LOG_DIRECT("[ES8388 Health @%s] Loi doc I2C!\r\n", tag);
        return;
    }
    LOG_DIRECT("[ES8388 Health @%s] R00=0x%02X R02=0x%02X R04=0x%02X R19=0x%02X\r\n", tag, r00, r02, r04, r19);
    if (r04 != 0x3C || (r00 & 0x06) == 0 || r19 != 0x00) {
        LOG_DIRECT("[ES8388] CANH BAO: Chip bi reset/sut ap! Dang khoi tao lai...\r\n");
        init_es8388();
    }
}



// ============================================================================
// HỆ THỐNG ÂM THANH RETRO CHIPTUNE TỔNG HỢP
// ============================================================================
typedef enum {
    SOUND_NONE = 0,
    SOUND_STARTUP,
    SOUND_DIGIT,
    SOUND_HEARTBEAT,
    SOUND_SMILEY,
    SOUND_PACMAN,
    SOUND_THUNDER,
    SOUND_EQUALIZER,
    SOUND_FIRE,
} sound_effect_t;

static QueueHandle_t s_sound_queue = NULL;

static void trigger_sound(sound_effect_t sfx) {
    if (s_sound_queue) {
        xQueueSend(s_sound_queue, &sfx, 0);
    }
}

static void play_silence(uint16_t duration_ms) {
    if (!s_tx_handle || duration_ms == 0) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        return;
    }
    int total_samples = (I2S_SAMPLE_RATE * duration_ms) / 1000;
    int16_t zeros[128 * 2] = {0};
    int samples_left = total_samples;
    while (samples_left > 0) {
        int chunk = (samples_left > 128) ? 128 : samples_left;
        size_t written = 0;
        if (i2s_channel_write(s_tx_handle, zeros, chunk * sizeof(int16_t) * 2, &written, 1000) != ESP_OK) {
            break;
        }
        samples_left -= chunk;
    }
}

static void play_tone(uint16_t freq, uint16_t duration_ms, int16_t volume) {
    if (!s_tx_handle || freq == 0) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));
        return;
    }
    int total_samples = (I2S_SAMPLE_RATE * duration_ms) / 1000;
    int16_t buf[128 * 2];
    uint32_t phase = 0;
    uint32_t phase_step = ((uint32_t)freq * 256 * 256) / I2S_SAMPLE_RATE;

    // Bien do ~1/2 (~10000-12000) ket hop -18dB analog -> tin hieu sach, khong bao hoa PAM8403
    int32_t safe_vol = volume / 2;
    int attack_len = I2S_SAMPLE_RATE / 100; // 10ms fade-in muot
    int decay_len  = I2S_SAMPLE_RATE / 60;  // ~16ms fade-out muot
    if (attack_len > total_samples / 8) attack_len = total_samples / 8;
    if (decay_len > total_samples / 4)  decay_len  = total_samples / 4;

    int samples_left = total_samples;
    while (samples_left > 0) {
        int chunk = (samples_left > 128) ? 128 : samples_left;
        for (int i = 0; i < chunk; i++) {
            int current_sample = total_samples - samples_left + i;
            int32_t current_vol = safe_vol;
            if (attack_len > 0 && current_sample < attack_len) {
                current_vol = (safe_vol * current_sample) / attack_len;
            } else if (decay_len > 0 && (samples_left - i) < decay_len) {
                current_vol = (safe_vol * (samples_left - i)) / decay_len;
            }

            uint8_t table_idx = (phase >> 8) & 0xFF;
            int16_t sample_val = (int16_t)(((int32_t)SINE_TABLE[table_idx] * current_vol) >> 15);
            buf[i * 2]     = sample_val;
            buf[i * 2 + 1] = sample_val;
            phase += phase_step;
        }
        size_t written = 0;
        esp_err_t werr = i2s_channel_write(s_tx_handle, buf, chunk * sizeof(int16_t) * 2, &written, 1000);
        if (werr != ESP_OK) {
            LOG_DIRECT("[AUDIO] Loi I2S write: %s\r\n", esp_err_to_name(werr));
            break;
        }
        samples_left -= chunk;
    }
}

static void play_startup_jingle(void) {
    play_tone(262, 200, 22000); // C4 am ap
    play_silence(40);
    play_tone(330, 200, 22000); // E4
    play_silence(40);
    play_tone(392, 200, 22000); // G4
    play_silence(40);
    play_tone(523, 380, 22000); // C5
}

static void play_digit_ding(void) {
    play_tone(392, 65, 14000); // G4 nhe nhang, khong choi tai
}

static void play_heartbeat_sfx(void) {
    play_tone(130, 80, 26000); // De nghe hon tren loa nho
    vTaskDelay(pdMS_TO_TICKS(60));
    play_tone(100, 100, 22000);
}

static void play_smiley_sfx(void) {
    for (int f = 600; f <= 1200; f += 75) {
        play_tone(f, 20, 20000);
    }
}

static void play_pacman_sfx(void) {
    for (int cycle = 0; cycle < 3; cycle++) {
        for (int f = 380; f <= 750; f += 65) {
            play_tone(f, 12, 22000);
        }
        for (int f = 750; f >= 380; f -= 65) {
            play_tone(f, 12, 22000);
        }
    }
}

static void play_thunder_sfx(void) {
    int total_samples = I2S_SAMPLE_RATE * 600 / 1000;
    int16_t buf[128 * 2];
    int samples_left = total_samples;
    uint32_t seed = 12345;
    int16_t filter = 0;

    while (samples_left > 0) {
        int chunk = (samples_left > 128) ? 128 : samples_left;
        int vol = (8500 * samples_left) / total_samples;

        for (int i = 0; i < chunk; i++) {
            seed = seed * 1103515245 + 12345;
            int16_t raw_noise = (int16_t)((seed >> 16) & 0x7FFF) - 16384;
            filter = (filter * 3 + raw_noise) / 4;
            int16_t val = (int16_t)(((int32_t)filter * vol) / 32768);
            buf[i * 2]     = val;
            buf[i * 2 + 1] = val;
        }
        size_t written = 0;
        esp_err_t werr = i2s_channel_write(s_tx_handle, buf, chunk * sizeof(int16_t) * 2, &written, 1000);
        if (werr != ESP_OK) {
            LOG_DIRECT("[AUDIO] Loi I2S write (thunder): %s\r\n", esp_err_to_name(werr));
            break;
        }
        samples_left -= chunk;
    }
}

static void play_equalizer_melody(void) {
    uint16_t notes[] = {220, 277, 330, 440, 330, 277, 220, 165};
    for (int i = 0; i < 8; i++) {
        play_tone(notes[i], 100, 22000);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void play_fire_crackles(void) {
    int total_samples = I2S_SAMPLE_RATE * 500 / 1000;
    int16_t buf[128 * 2];
    int samples_left = total_samples;
    uint32_t seed = 54321;
    while (samples_left > 0) {
        int chunk = (samples_left > 128) ? 128 : samples_left;
        for (int i = 0; i < chunk; i++) {
            seed = seed * 1664525 + 1013904223;
            int16_t val = 0;
            if ((seed & 0xFFF) < 15) {
                val = (seed & 0x1) ? 7000 : -7000;
            } else if ((seed & 0xFF) < 35) {
                val = (int16_t)(((seed & 0x7FF) - 1024) / 2);
            }
            buf[i * 2]     = val;
            buf[i * 2 + 1] = val;
        }
        size_t written = 0;
        esp_err_t werr = i2s_channel_write(s_tx_handle, buf, chunk * sizeof(int16_t) * 2, &written, 1000);
        if (werr != ESP_OK) {
            LOG_DIRECT("[AUDIO] Loi I2S write (fire): %s\r\n", esp_err_to_name(werr));
            break;
        }
        samples_left -= chunk;
    }
}

static void audio_task(void *pvParameters) {
    sound_effect_t sfx;
    uint32_t sfx_count = 0;
    while (1) {
        if (xQueueReceive(s_sound_queue, &sfx, portMAX_DELAY) == pdTRUE) {
            sfx_count++;
            if (sfx_count <= 5) {
                LOG_DIRECT("[AUDIO] Dang phat hieu ung #%d (count=%lu)...\r\n", (int)sfx, (unsigned long)sfx_count);
            }
            switch (sfx) {
                case SOUND_STARTUP:   play_startup_jingle(); break;
                case SOUND_DIGIT:     play_digit_ding(); break;
                case SOUND_HEARTBEAT: play_heartbeat_sfx(); break;
                case SOUND_SMILEY:    play_smiley_sfx(); break;
                case SOUND_PACMAN:    play_pacman_sfx(); break;
                case SOUND_THUNDER:   play_thunder_sfx(); break;
                case SOUND_EQUALIZER: play_equalizer_melody(); break;
                case SOUND_FIRE:      play_fire_crackles(); break;
                default: break;
            }
            if (sfx_count <= 5) {
                LOG_DIRECT("[AUDIO] Xong hieu ung #%d (count=%lu)\r\n", (int)sfx, (unsigned long)sfx_count);
            }
        }
    }
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

static void draw_char(char c, bool r, bool g, bool b) {
    int idx = -1;
    if (c >= '0' && c <= '9') idx = c - '0';
    else if (c >= 'A' && c <= 'Z') idx = c - 'A' + 10;
    else if (c >= 'a' && c <= 'z') idx = c - 'a' + 10;

    if (idx >= 0 && idx < 36) {
        draw_bitmap(FONT_16X16[idx], r, g, b);
    }
}

// 1. TEST MÀU TOÀN MÀN HÌNH
static void test_full_colors(void) {
    LOG_DIRECT(">>> 1. TEST TOAN MAN HINH: RED -> GREEN -> BLUE -> WHITE <<<\r\n");

    fb_clear();
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) draw_pixel(x, y, true, false, false);
    trigger_sound(SOUND_DIGIT);
    fb_render(); vTaskDelay(pdMS_TO_TICKS(1000));

    fb_clear();
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) draw_pixel(x, y, false, true, false);
    trigger_sound(SOUND_DIGIT);
    fb_render(); vTaskDelay(pdMS_TO_TICKS(1000));

    fb_clear();
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) draw_pixel(x, y, false, false, true);
    trigger_sound(SOUND_SMILEY);
    fb_render(); vTaskDelay(pdMS_TO_TICKS(1000));

    fb_clear();
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) draw_pixel(x, y, true, true, true);
    trigger_sound(SOUND_EQUALIZER);
    fb_render(); vTaskDelay(pdMS_TO_TICKS(1000));
}

// 2. CHẠY CHỮ SỐ 0 -> 9 (KÈM ÂM THANH DING)
static void test_digits_0_to_9(void) {
    LOG_DIRECT(">>> 2. TEST CHU SO 0 -> 9 (Kem am thanh Ding) <<<\r\n");
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
        trigger_sound(SOUND_DIGIT);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(650));
    }
}

// 3. CHẠY TỪ KHÓA CHỮ CÁI: E-S-P-3-2  O-K
static void test_words_esp_ok(void) {
    LOG_DIRECT(">>> 3. TEST CHU CAI: E-S-P-3-2  O-K <<<\r\n");
    const char text[] = "ESP32OK";
    for (int i = 0; text[i] != '\0'; i++) {
        fb_clear();
        draw_char(text[i], true, true, false);
        trigger_sound(SOUND_DIGIT);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

// 4. BỘ 6 HOẠT HÌNH PIXEL ART
static const uint16_t HEART_BIG[16] = {
    0x0000, 0x0E70, 0x3FFC, 0x7FFE, 0x7FFE, 0x7FFE, 0x3FFC, 0x3FFC,
    0x1FF8, 0x0FF0, 0x07E0, 0x03C0, 0x0180, 0x0000, 0x0000, 0x0000
};
static const uint16_t HEART_SMALL[16] = {
    0x0000, 0x0000, 0x0660, 0x0FF0, 0x0FF0, 0x0FF0, 0x07E0, 0x07E0,
    0x03C0, 0x0180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static void anim_beating_heart(int duration_ms) {
    LOG_DIRECT("  [Animation] 1. Trai tim dap (Kem tieng tim dap)...\r\n");
    int elapsed = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        draw_bitmap(HEART_BIG, true, false, false);
        draw_pixel(4, 3, true, true, true);
        draw_pixel(4, 4, true, true, true);
        trigger_sound(SOUND_HEARTBEAT);
        fb_render(); vTaskDelay(pdMS_TO_TICKS(220));

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

        elapsed += (220 + 140 + 200 + 450);
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
    LOG_DIRECT("  [Animation] 2. Mat cuoi nhay mat (Kem tieng Chirp)...\r\n");
    int elapsed = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        draw_bitmap(SMILEY_OPEN, true, true, false);
        fb_render(); vTaskDelay(pdMS_TO_TICKS(800));

        fb_clear();
        draw_bitmap(SMILEY_WINK, true, true, false);
        trigger_sound(SOUND_SMILEY);
        fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

        elapsed += 1100;
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
    LOG_DIRECT("  [Animation] 3. Pac-Man an keo (Kem am thanh Waka-waka)...\r\n");
    int elapsed = 0, step = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        if (step % 2 == 0) {
            draw_bitmap(PACMAN_OPEN, true, true, false);
            trigger_sound(SOUND_PACMAN);
        } else {
            draw_bitmap(PACMAN_CLOSED, true, true, false);
        }

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
    LOG_DIRECT("  [Animation] 4. May mua & Sam chop (Kem tieng sam)... \r\n");
    int elapsed = 0, drop_frame = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        draw_bitmap(CLOUD_BMP, false, true, true);

        for (int x = 2; x < 15; x += 3) {
            int y = 8 + ((drop_frame + x) % 8);
            draw_pixel(x, y, false, false, true);
        }

        if (drop_frame % 14 == 0) {
            // Sam chop vang ruc
            draw_pixel(7, 8, true, true, false);
            draw_pixel(8, 9, true, true, false);
            draw_pixel(7, 10, true, true, false);
            draw_pixel(6, 11, true, true, false);
            draw_pixel(7, 12, true, true, false);
            draw_pixel(8, 13, true, true, false);
            trigger_sound(SOUND_THUNDER);
        }

        fb_render(); vTaskDelay(pdMS_TO_TICKS(120));
        elapsed += 120; drop_frame++;
    }
}

static void anim_equalizer(int duration_ms) {
    LOG_DIRECT("  [Animation] 5. Cot song Equalizer (Kem giai dieu 8-bit)...\r\n");
    int elapsed = 0;
    int peaks[16] = {0};
    trigger_sound(SOUND_EQUALIZER);

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
    LOG_DIRECT("  [Animation] 6. Ngon lua Doom Fire (Kem tieng lua reo)...\r\n");
    int elapsed = 0;
    uint8_t fire[16][16] = {0};
    trigger_sound(SOUND_FIRE);

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
        LOG_DIRECT("=== KHOI TAO HT16D35A THANH CONG! ===\r\n");
    }

    // Khởi tạo I2S và DAC ES8388
    init_i2s();
    init_es8388();

    // Phat nhac khoi dong C5-E5-G5-C6 luc tat ca LED dang tat (kiem tra nhieu/sut ap)
    LOG_DIRECT("[AUDIO] Test nhac khoi dong (C5-E5-G5-C6) luc LED dang tat...\r\n");
    trigger_sound(SOUND_STARTUP);
    vTaskDelay(pdMS_TO_TICKS(1300));

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
                if (!s_es8388_inited) {
                    init_es8388();
                }
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
        } else {
            if (!s_es8388_inited) {
                init_es8388();
            }
            // 1. Test màu toàn màn hình (R, G, B, White)
            test_full_colors();
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));
            check_es8388_health("AfterColors");

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
            check_es8388_health("EndRound");
            LOG_DIRECT(">>> Hoan thanh vong lap trinh dien thu: %lu <<<\r\n", (unsigned long)round);
        }
    }
}

extern "C" void app_main(void) {
    // Đặt stdout sang Non-blocking
    int flags = fcntl(fileno(stdout), F_GETFL, 0);
    fcntl(fileno(stdout), F_SETFL, flags | O_NONBLOCK);

    vTaskDelay(pdMS_TO_TICKS(1000));

    LOG_DIRECT("\r\n\r\n======================================================\r\n");
    LOG_DIRECT("  FIRMWARE MA TRAN LED 16x16 RGB + AM THANH ES8388 DAC\r\n");
    LOG_DIRECT("  LED: CLK=GPIO 1, DIO=GPIO 0 | I2C: SDA=GPIO 20, SCL=GPIO 21\r\n");
    LOG_DIRECT("  I2S DAC: MCLK=GPIO 8, BCLK=GPIO 5, DOUT=GPIO 6, WS=GPIO 7\r\n");
    LOG_DIRECT("======================================================\r\n\r\n");

    // 1. Cấu hình chân SPI HT16D35A
    gpio_reset_pin(PIN_CLK);
    gpio_reset_pin(PIN_DIO);
    gpio_set_direction(PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_CLK, 0);
    gpio_set_level(PIN_DIO, 0);



    // 3. Khởi tạo hàng đợi âm thanh
    s_sound_queue = xQueueCreate(10, sizeof(sound_effect_t));

    // 4. Tạo task âm thanh chạy độc lập
    xTaskCreate(audio_task, "audio_task", 4096, NULL, 6, NULL);

    // 5. Chạy task điều khiển hiển thị
    xTaskCreate(display_task, "display_task", 6144, NULL, 5, NULL);
}
