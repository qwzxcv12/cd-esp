/**
 * ============================================================================
 * TRÌNH DIỄN ĐỒ HỌA HOẠT HÌNH PIXEL ART 16x16 FULL RGB (ESP-IDF v5.1)
 * 4x HOLTEK HT16D35A KẾT HỢP VỚI IC MỞ RỘNG PI4IOE5V6416 (TQFN-24)
 * ============================================================================
 * SƠ ĐỒ ĐẤU NỐI ĐÃ XÁC ĐỊNH CHÍNH XÁC (KHÔNG DÙNG CHÂN STRAPPING GPIO 9):
 * 
 * 1. TÍN HIỆU SPI TRỰC TIẾP TỪ ESP32-C3:
 *    - CLK -> GPIO 1 trên ESP32-C3 (Xung Clock chung 4 IC HT16D35A)
 *    - DIO -> GPIO 0 trên ESP32-C3 (Dữ liệu Data chung 4 IC HT16D35A)
 * 
 * 2. GIAO TIẾP I2C VỚI PI4IOE5V6416:
 *    - SDA -> GPIO 20 trên ESP32-C3 (Chân 20 của PI4IOE5V6416)
 *    - SCL -> GPIO 21 trên ESP32-C3 (Chân 19 của PI4IOE5V6416)
 * 
 * 3. ĐIỀU KHIỂN CHỌN CHIP CS1..CS4 QUA PORT 0 CỦA PI4IOE5V6416:
 *    - CS1 (IC 1) -> Chân 4 của PI4IOE5V6416 (P0_3, Bit 3)
 *    - CS2 (IC 2) -> Chân 3 của PI4IOE5V6416 (P0_2, Bit 2)
 *    - CS3 (IC 3) -> Chân 2 của PI4IOE5V6416 (P0_1, Bit 1)
 *    - CS4 (IC 4) -> Chân 1 của PI4IOE5V6416 (P0_0, Bit 0)
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "RGB_MATRIX";

// --- CẤU HÌNH CHÂN SPI CHO HT16D35A ---
#define PIN_CLK         GPIO_NUM_1
#define PIN_DIO         GPIO_NUM_0

// --- CẤU HÌNH CHÂN I2C CHO PI4IOE5V6416 ---
#define I2C_SDA_PIN     GPIO_NUM_20
#define I2C_SCL_PIN     GPIO_NUM_21
#define I2C_PORT_NUM    I2C_NUM_0
#define I2C_FREQ_HZ     100000       // 100 kHz chuẩn, ổn định chống nhiễu

#define NUM_ICS         4
#define NUM_ROWS_PER_IC 28

// Ánh xạ bit CS trên Port 0 của PI4IOE5V6416 (Active LOW)
// CS1 -> Bit 3, CS2 -> Bit 2, CS3 -> Bit 1, CS4 -> Bit 0
static const uint8_t CS_BITS[NUM_ICS] = {3, 2, 1, 0};

static uint8_t s_i2c_addr = 0x20;
static bool s_i2c_ready = false;
static uint8_t s_port0_state = 0xFF; // Mặc định tất cả CS kéo lên HIGH

static uint8_t fb[NUM_ICS][NUM_ROWS_PER_IC];

static inline void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

// Ghi 1 thanh ghi vào PI4IOE5V6416
static esp_err_t i2c_write_byte(uint8_t reg, uint8_t val) {
    if (!s_i2c_ready) return ESP_FAIL;
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(I2C_PORT_NUM, s_i2c_addr, buf, 2, pdMS_TO_TICKS(15));
}

// Kéo CS của IC tương ứng xuống mức LOW (Chọn chip)
static inline void cs_select(uint8_t ic_idx) {
    if (ic_idx >= NUM_ICS || !s_i2c_ready) return;
    s_port0_state &= ~(1 << CS_BITS[ic_idx]);
    i2c_write_byte(0x02, s_port0_state);
    delay_us(2);
}

// Kéo CS của IC tương ứng lên mức HIGH (Bỏ chọn chip)
static inline void cs_deselect(uint8_t ic_idx) {
    if (ic_idx >= NUM_ICS || !s_i2c_ready) return;
    s_port0_state |= (1 << CS_BITS[ic_idx]);
    i2c_write_byte(0x02, s_port0_state);
    delay_us(2);
}

// Gửi 1 byte SPI mềm ra chân DIO theo xung CLK
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
    printf("[HT16D35A] IC %d (CS%d) khoi tao OK!\n", ic_idx + 1, ic_idx + 1);
}

// Kiểm tra sự tồn tại của thiết bị I2C tại địa chỉ addr
static bool check_i2c_device(uint8_t addr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

static void init_i2c_expander(void) {
    printf("[I2C] Khoi tao I2C Master tren SDA=GPIO %d, SCL=GPIO %d (Toc do %d Hz)...\n",
           I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA_PIN;
    conf.scl_io_num = I2C_SCL_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FREQ_HZ;

    esp_err_t err = i2c_param_config(I2C_PORT_NUM, &conf);
    if (err == ESP_OK) {
        err = i2c_driver_install(I2C_PORT_NUM, conf.mode, 0, 0, 0);
    }

    if (err != ESP_OK) {
        printf("[I2C] LOI CAI DAT DRIVER I2C: %d\n", err);
        return;
    }

    // 1. Kiểm tra địa chỉ 0x20 hoặc 0x21
    if (check_i2c_device(0x20)) {
        s_i2c_addr = 0x20;
        s_i2c_ready = true;
        printf("[I2C] >>> TIM THAY PI4IOE5V6416 TAI DIA CHI 0x20 <<<\n");
    } else if (check_i2c_device(0x21)) {
        s_i2c_addr = 0x21;
        s_i2c_ready = true;
        printf("[I2C] >>> TIM THAY PI4IOE5V6416 TAI DIA CHI 0x21 <<<\n");
    } else {
        printf("[I2C] CANH BAO: Khong tim thay thiet bi tai 0x20 hoac 0x21!\n");
        printf("[I2C] Dang quet toan bo bus I2C (0x08 - 0x77)...\n");
        bool any_found = false;
        for (uint8_t a = 0x08; a < 0x78; a++) {
            if (check_i2c_device(a)) {
                printf("  -> Phat hien thiet bi tai dia chi I2C: 0x%02X\n", a);
                s_i2c_addr = a;
                s_i2c_ready = true;
                any_found = true;
                break;
            }
        }
        if (!any_found) {
            printf("[I2C] KHONG CO THIET BI NAO PHAN HOI! Kiem tra lai day noi SDA=GPIO20, SCL=GPIO21 va nguon chip!\n");
            return;
        }
    }

    // 2. Cấu hình các thanh ghi cho PI4IOE5V6416
    s_port0_state = 0xFF; // Mặc định tất cả chân HIGH
    i2c_write_byte(0x02, s_port0_state); // Output Port 0 = 0xFF (CS1..CS4 HIGH)
    i2c_write_byte(0x03, 0xFF);         // Output Port 1 = 0xFF
    i2c_write_byte(0x06, 0x00);         // Config Port 0: 0 = OUTPUT
    i2c_write_byte(0x07, 0x00);         // Config Port 1: 0 = OUTPUT
    i2c_write_byte(0x4F, 0x00);         // Push-Pull output
    printf("[I2C] Cau hinh PI4IOE5V6416 Port 0 lam ngo ra CS thanh cong!\n");
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

// 1. TRÁI TIM ĐẬP (BEATING HEART)
static const uint16_t HEART_BIG[16] = {
    0x0000, 0x0E70, 0x3FFC, 0x7FFE, 0x7FFE, 0x7FFE, 0x3FFC, 0x3FFC,
    0x1FF8, 0x0FF0, 0x07E0, 0x03C0, 0x0180, 0x0000, 0x0000, 0x0000
};

static const uint16_t HEART_SMALL[16] = {
    0x0000, 0x0000, 0x0660, 0x0FF0, 0x0FF0, 0x0FF0, 0x07E0, 0x07E0,
    0x03C0, 0x0180, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

static void anim_beating_heart(int duration_ms) {
    printf("[Animation] Hieu ung 1: Trai tim dap...\n");
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

// 2. MẶT CƯỜI NHÁY MẮT (WINKING SMILEY)
static const uint16_t SMILEY_OPEN[16] = {
    0x07E0, 0x1FF8, 0x3FFC, 0x73CE, 0x73CE, 0xFFFF, 0xFFFF, 0xFFFF,
    0xE007, 0xF00F, 0x7C3E, 0x7FFE, 0x3FFC, 0x1FF8, 0x07E0, 0x0000
};

static const uint16_t SMILEY_WINK[16] = {
    0x07E0, 0x1FF8, 0x3FFC, 0x73FE, 0x7386, 0xFFFF, 0xFFFF, 0xFFFF,
    0xE007, 0xF00F, 0x7C3E, 0x7FFE, 0x3FFC, 0x1FF8, 0x07E0, 0x0000
};

static void anim_winking_smiley(int duration_ms) {
    printf("[Animation] Hieu ung 2: Mat cuoi nhay mat...\n");
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

// 3. PAC-MAN HÁ MỒM ĂN KẸO
static const uint16_t PACMAN_OPEN_RIGHT[16] = {
    0x03C0, 0x0FF0, 0x1FF8, 0x3FFC, 0x7FE0, 0x7F80, 0x7E00, 0x7C00,
    0x7C00, 0x7E00, 0x7F80, 0x7FE0, 0x3FFC, 0x1FF8, 0x0FF0, 0x03C0
};

static const uint16_t PACMAN_CLOSED[16] = {
    0x03C0, 0x0FF0, 0x1FF8, 0x3FFC, 0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE,
    0x7FFE, 0x7FFE, 0x7FFE, 0x7FFE, 0x3FFC, 0x1FF8, 0x0FF0, 0x03C0
};

static void anim_pacman(int duration_ms) {
    printf("[Animation] Hieu ung 3: Pac-Man...\n");
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
    printf("[Animation] Hieu ung 4: May mua roi & sam chop...\n");
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
    printf("[Animation] Hieu ung 5: Cot song am nhac (Equalizer)...\n");
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
    printf("[Animation] Hieu ung 6: Ngon lua bap bung (Doom Fire)...\n");
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
    // Đợi 500ms để mọi thứ ổn định
    vTaskDelay(pdMS_TO_TICKS(500));

    if (s_i2c_ready) {
        printf("=== BAT DAU KHOI TAO 4 CHIP HT16D35A QUA PI4IOE5V6416 ===\n");
        for (int i = 0; i < NUM_ICS; i++) {
            init_ht16d35a(i);
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        printf("=== KHOI TAO 4 CHIP HT16D35A HOAN TAT! ===\n");
    } else {
        printf("[CANH BAO] Khong the khoi tao HT16D35A vi chua ket noi duoc PI4IOE5V6416!\n");
    }

    uint32_t loop_count = 0;
    while (1) {
        if (s_i2c_ready) {
            anim_beating_heart(6000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            anim_winking_smiley(6000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            anim_pacman(6000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            anim_rain_and_thunder(6500);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            anim_equalizer(8000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(300));

            anim_doom_fire(8000);
            fb_clear(); fb_render(); vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            // Nếu chưa tìm thấy I2C, thử scan lại định kỳ mỗi 3 giây
            printf("[Cho ket noi] Dang kiem tra lai chip PI4IOE5V6416 tren SDA=GPIO 20, SCL=GPIO 21...\n");
            init_i2c_expander();
            vTaskDelay(pdMS_TO_TICKS(3000));
        }

        loop_count++;
        printf("[Trang thai] ESP32-C3 dang chay binh thuong. Chu ky: %lu\n", (unsigned long)loop_count);
    }
}

extern "C" void app_main(void) {
    // 1. Chờ 1.5 giây để USB-Serial-JTAG kết nối ổn định trên Windows
    vTaskDelay(pdMS_TO_TICKS(1500));

    printf("\n\n");
    printf("=====================================================================\n");
    printf("  FIRMWARE MA TRAN LED 16x16 FULL RGB (ESP32-C3 + PI4IOE5V6416)\n");
    printf("  CLK=GPIO 1 | DIO=GPIO 0 | SDA=GPIO 20 | SCL=GPIO 21\n");
    printf("=====================================================================\n\n");

    // 2. Cấu hình chân SPI mềm (CLK & DIO)
    gpio_reset_pin(PIN_CLK);
    gpio_reset_pin(PIN_DIO);
    gpio_set_direction(PIN_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_DIO, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_CLK, 0);
    gpio_set_level(PIN_DIO, 0);
    printf("[GPIO] Khoi tao CLK=GPIO 1, DIO=GPIO 0 thanh cong.\n");

    // 3. Khởi tạo giao tiếp I2C với PI4IOE5V6416
    init_i2c_expander();

    // 4. Tạo task trình diễn hiệu ứng hoạt hình (Stack 6144 bytes rộng rãi)
    xTaskCreate(animation_task, "anim_task", 6144, NULL, 5, NULL);
}
