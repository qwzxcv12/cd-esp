/**
 * ============================================================================
 * TRÌNH DIỄN ĐỒ HỌA HOẠT HÌNH PIXEL ART ĐỈNH CAO TRÊN MA TRẬN 16x16 FULL RGB
 * (4x HOLTEK HT16D35A KẾT HỢP VỚI IC MỞ RỘNG PI4IOE5V6416 - ESP32-C3)
 * ============================================================================
 * ÁNH XẠ CHÂN CS QUA PI4IOE5V6416 (ĐÃ ĐO THỰC TẾ):
 * - CS1 (IC 1) -> Chân 4 của PI4IOE5V6416 (Port 0 - Bit 3)
 * - CS2 (IC 2) -> Chân 3 của PI4IOE5V6416 (Port 0 - Bit 2)
 * - CS3 (IC 3) -> Chân 2 của PI4IOE5V6416 (Port 0 - Bit 1)
 * - CS4 (IC 4) -> Chân 1 của PI4IOE5V6416 (Port 0 - Bit 0)
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "HT16D35A_PI4IOE";

// --- CHÂN TÍN HIỆU SPI TRỰC TIẾP TỪ ESP32-C3 ---
#define PIN_CLK         GPIO_NUM_1   // Xung Clock nối chung 4 IC HT16D35A
#define PIN_DIO         GPIO_NUM_0   // Dữ liệu Data nối chung 4 IC HT16D35A

#define NUM_ICS         4
#define I2C_NUM         I2C_NUM_0
#define I2C_FREQ_HZ     400000       // Tốc độ Fast-Mode 400 kHz

// Ánh xạ bit chọn chip trên Port 0 của PI4IOE5V6416
// IC 0 (CS1) -> Bit 3 (Chân 4)
// IC 1 (CS2) -> Bit 2 (Chân 3)
// IC 2 (CS3) -> Bit 1 (Chân 2)
// IC 3 (CS4) -> Bit 0 (Chân 1)
static const uint8_t CS_BITS[NUM_ICS] = {3, 2, 1, 0};
static uint8_t s_port0_state = 0xFF; // Mặc định tất cả CS đều ở mức HIGH (1)
static uint8_t s_i2c_addr = 0x20;

typedef struct {
    gpio_num_t sda;
    gpio_num_t scl;
} i2c_pair_t;

static const i2c_pair_t CANDIDATE_PAIRS[] = {
    {GPIO_NUM_4, GPIO_NUM_5},
    {GPIO_NUM_5, GPIO_NUM_4},
    {GPIO_NUM_0, GPIO_NUM_1},
    {GPIO_NUM_1, GPIO_NUM_0},
    {GPIO_NUM_20, GPIO_NUM_21},
    {GPIO_NUM_21, GPIO_NUM_20},
    {GPIO_NUM_8, GPIO_NUM_9},
    {GPIO_NUM_9, GPIO_NUM_8},
    {GPIO_NUM_10, GPIO_NUM_9},
    {GPIO_NUM_6, GPIO_NUM_7},
    {GPIO_NUM_2, GPIO_NUM_3}
};
#define NUM_CANDIDATE_PAIRS (sizeof(CANDIDATE_PAIRS) / sizeof(CANDIDATE_PAIRS[0]))

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

#define NUM_ROWS_PER_IC         28

static uint8_t fb[NUM_ICS][NUM_ROWS_PER_IC];

static inline void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

// Ghi dữ liệu vào thanh ghi PI4IOE5V6416 qua I2C
static inline void i2c_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_master_write_to_device(I2C_NUM, s_i2c_addr, buf, 2, pdMS_TO_TICKS(10));
}

// Kéo chân CS tương ứng xuống LOW (Chọn IC)
static inline void cs_select(uint8_t ic_idx) {
    if (ic_idx >= NUM_ICS) return;
    s_port0_state &= ~(1 << CS_BITS[ic_idx]);
    i2c_write_reg(0x02, s_port0_state);
    delay_us(2);
}

// Kéo chân CS tương ứng lên HIGH (Hủy chọn IC)
static inline void cs_deselect(uint8_t ic_idx) {
    if (ic_idx >= NUM_ICS) return;
    s_port0_state |= (1 << CS_BITS[ic_idx]);
    i2c_write_reg(0x02, s_port0_state);
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
    for (uint8_t ic = 0; ic < NUM_ICS; ic++) {
        cs_select(ic);
        write_byte(CMD_WRITE_DDRAM);
        write_byte(0x00);
        for (int r = 0; r < NUM_ROWS_PER_IC; r++) {
            write_byte(fb[ic][r]);
        }
        cs_deselect(ic);
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

    cs_select(ic_idx);
    write_byte(CMD_WRITE_DDRAM);
    write_byte(0x00);
    for (int r = 0; r < NUM_ROWS_PER_IC; r++) write_byte(0x00);
    cs_deselect(ic_idx);

    send_cmd_2b(ic_idx, CMD_SYSTEM_CTRL, 0x03);
    ESP_LOGI(TAG, "IC %d (CS%d) khoi tao thanh cong!", ic_idx + 1, ic_idx + 1);
}

static bool test_i2c_connection(uint8_t addr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_NUM, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

static void init_hardware(void) {
    // 1. Cấu hình chân Clock & Data cho HT16D35A
    gpio_reset_pin(PIN_CLK);
    gpio_reset_pin(PIN_DIO);
    gpio_set_direction(PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_CLK, 0);
    gpio_set_level(PIN_DIO, 0);

    // 2. Tự động nhận diện I2C bus cho PI4IOE5V6416
    ESP_LOGI(TAG, "Khoi tao bus I2C ket noi PI4IOE5V6416...");
    bool found = false;

    for (size_t i = 0; i < NUM_CANDIDATE_PAIRS; i++) {
        gpio_num_t sda = CANDIDATE_PAIRS[i].sda;
        gpio_num_t scl = CANDIDATE_PAIRS[i].scl;

        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = sda;
        conf.scl_io_num = scl;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = I2C_FREQ_HZ;

        i2c_param_config(I2C_NUM, &conf);
        if (i2c_driver_install(I2C_NUM, conf.mode, 0, 0, 0) != ESP_OK) continue;

        if (test_i2c_connection(0x20)) {
            s_i2c_addr = 0x20;
            found = true;
            ESP_LOGI(TAG, ">>> Da ket noi PI4IOE5V6416 (0x20) tren SDA=GPIO %d, SCL=GPIO %d <<<", sda, scl);
            break;
        }
        if (test_i2c_connection(0x21)) {
            s_i2c_addr = 0x21;
            found = true;
            ESP_LOGI(TAG, ">>> Da ket noi PI4IOE5V6416 (0x21) tren SDA=GPIO %d, SCL=GPIO %d <<<", sda, scl);
            break;
        }

        i2c_driver_delete(I2C_NUM);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (!found) {
        ESP_LOGW(TAG, "Dung cau hinh I2C mac dinh: SDA=GPIO 4, SCL=GPIO 5");
        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = GPIO_NUM_4;
        conf.scl_io_num = GPIO_NUM_5;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = I2C_FREQ_HZ;
        i2c_param_config(I2C_NUM, &conf);
        i2c_driver_install(I2C_NUM, conf.mode, 0, 0, 0);
    }

    // 3. Cấu hình PI4IOE5V6416: Port 0 làm OUTPUT và đặt sẵn mức HIGH (1)
    s_port0_state = 0xFF;
    i2c_write_reg(0x02, s_port0_state); // Output Port 0 = 0xFF
    i2c_write_reg(0x03, 0xFF);         // Output Port 1 = 0xFF
    i2c_write_reg(0x06, 0x00);         // Config Port 0: 0 = OUTPUT
    i2c_write_reg(0x07, 0x00);         // Config Port 1: 0 = OUTPUT
    i2c_write_reg(0x4F, 0x00);         // Push-Pull output cho cạnh xung vuông đẹp
}

// ============================================================================
// HỆ THỐNG ĐỒ HỌA MA TRẬN 16x16 CHUẨN XÁC 100%
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

// 1. TRÁI TIM ĐẬP
static void anim_beating_heart(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 1: TRAI TIM DAP (BEATING HEART) <<<");
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

// 2. MẶT CƯỜI NHÁY MẮT
static const uint16_t SMILEY_OPEN[16] = {
    0x07E0, 0x1FF8, 0x3FFC, 0x73CE, 0x73CE, 0xFFFF, 0xFFFF, 0xFFFF,
    0xE007, 0xF00F, 0x7C3E, 0x7FFE, 0x3FFC, 0x1FF8, 0x07E0, 0x0000
};

static const uint16_t SMILEY_WINK[16] = {
    0x07E0, 0x1FF8, 0x3FFC, 0x73FE, 0x7386, 0xFFFF, 0xFFFF, 0xFFFF,
    0xE007, 0xF00F, 0x7C3E, 0x7FFE, 0x3FFC, 0x1FF8, 0x07E0, 0x0000
};

static void anim_winking_smiley(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 2: MAT CUOI NHAY MAT (WINKING SMILEY) <<<");
    int elapsed = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        draw_bitmap(SMILEY_OPEN, true, true, false);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(900));

        fb_clear();
        draw_bitmap(SMILEY_WINK, true, true, false);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(250));

        elapsed += 1150;
    }
}

// 3. PAC-MAN ĂN KẸO
static const uint16_t PACMAN_OPEN_RIGHT[16] = {
    0x03C0, 0x0FF0, 0x1FF8, 0x3FFC, 0x7FE0, 0x7F80, 0x7E00, 0x7C00,
    0x7C00, 0x7E00, 0x7F80, 0x7FE0, 0x3FFC, 0x1FF8, 0x0FF0, 0x03C0
};

static const uint16_t PACMAN_CLOSED[16] = {
    0x03C0, 0x0FF0, 0x1FF8, 0x3FFC, 0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE,
    0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE, 0x3FFC, 0x1FF8, 0x0FF0, 0x03C0
};

static void anim_pacman(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 3: PAC-MAN HA MOM AN KEO <<<");
    int elapsed = 0;
    int step = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        if (step % 2 == 0) {
            draw_bitmap(PACMAN_OPEN_RIGHT, true, true, false);
        } else {
            draw_bitmap(PACMAN_CLOSED, true, true, false);
        }
        draw_pixel(13, 7, false, true, true);
        draw_pixel(14, 7, false, true, true);
        draw_pixel(13, 8, false, true, true);
        draw_pixel(14, 8, false, true, true);

        fb_render();
        vTaskDelay(pdMS_TO_TICKS(180));
        elapsed += 180;
        step++;
    }
}

// 4. MÂY MƯA RƠI & SẤM CHỚP
static const uint16_t CLOUD_BMP[16] = {
    0x0000, 0x0000, 0x0780, 0x1FE0, 0x3FF8, 0x7FFC, 0xFFFF, 0xFFFF,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static void anim_rain_and_thunder(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 4: MAY MUA ROI & SAM CHOP <<<");
    int elapsed = 0;
    int drop_frame = 0;
    while (elapsed < duration_ms) {
        fb_clear();
        draw_bitmap(CLOUD_BMP, false, true, true);

        for (int x = 2; x < 15; x += 3) {
            int y = 8 + ((drop_frame + x) % 8);
            draw_pixel(x, y, false, false, true);
        }

        if (drop_frame % 15 == 0) {
            draw_pixel(7, 8, true, true, false);
            draw_pixel(8, 9, true, true, false);
            draw_pixel(7, 10, true, true, false);
            draw_pixel(6, 11, true, true, false);
            draw_pixel(7, 12, true, true, false);
            draw_pixel(8, 13, true, true, false);
            draw_pixel(7, 14, true, true, false);
        }

        fb_render();
        vTaskDelay(pdMS_TO_TICKS(120));
        elapsed += 120;
        drop_frame++;
    }
}

// 5. CỘT SÓNG ÂM NHẠC (EQUALIZER)
static void anim_equalizer(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 5: COT SONG AM NHAC (EQUALIZER) <<<");
    int elapsed = 0;
    int peaks[16] = {0};

    while (elapsed < duration_ms) {
        fb_clear();
        for (int x = 0; x < 16; x++) {
            int height = 2 + (rand() % 14);
            if (height > peaks[x]) peaks[x] = height;
            else if (peaks[x] > 0) peaks[x]--;

            for (int y = 15; y >= (15 - height); y--) {
                if (y >= 11) draw_pixel(x, y, false, true, false);
                else if (y >= 5) draw_pixel(x, y, true, true, false);
                else draw_pixel(x, y, true, false, false);
            }
            if (peaks[x] > 0) {
                int peak_y = 15 - peaks[x];
                draw_pixel(x, peak_y, true, true, true);
            }
        }
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(90));
        elapsed += 90;
    }
}

// 6. NGỌN LỬA BẬP BÙNG (DOOM FIRE)
static void anim_doom_fire(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 6: NGON LUA BAP BUNG (DOOM FIRE) <<<");
    int elapsed = 0;
    uint8_t fire[16][16] = {0};

    while (elapsed < duration_ms) {
        fb_clear();
        for (int x = 0; x < 16; x++) {
            fire[x][15] = (rand() % 2 == 0) ? 3 : 2;
        }

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

        fb_render();
        vTaskDelay(pdMS_TO_TICKS(80));
        elapsed += 80;
    }
}

// ============================================================================
// TASK ĐIỀU KHIỂN CHÍNH
// ============================================================================
static void animation_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== KHOI DONG TRINH DIEN DO HOA 16x16 FULL RGB (PI4IOE5V6416) ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        // 1. TRÁI TIM ĐẬP (6 giây)
        anim_beating_heart(6000);
        fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

        // 2. MẶT CƯỜI NHÁY MẮT (6 giây)
        anim_winking_smiley(6000);
        fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

        // 3. PAC-MAN HÁ MỒM ĂN KẸO (6 giây)
        anim_pacman(6000);
        fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

        // 4. MÂY MƯA RƠI & SẤM CHỚP (6 giây)
        anim_rain_and_thunder(6500);
        fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

        // 5. CỘT SÓNG ÂM NHẠC (8 giây)
        anim_equalizer(8000);
        fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

        // 6. NGỌN LỬA BẬP BÙNG (8 giây)
        anim_doom_fire(8000);
        fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(500));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "ESP32-C3 16x16 RGB PIXEL ART WITH PI4IOE5V6416");
    init_hardware();
    xTaskCreate(animation_task, "anim_task", 4096, NULL, 5, NULL);
}
