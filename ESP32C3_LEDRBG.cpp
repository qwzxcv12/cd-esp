/**
 * ============================================================================
 * TRÌNH DIỄN ĐỒ HỌA HOẠT HÌNH PIXEL ART ĐỈNH CAO TRÊN MA TRẬN 16x16 FULL RGB
 * (4x HOLTEK HT16D35A - ESP32-C3)
 * ============================================================================
 * 6 HIỆU ỨNG ĐỘNG RÕ NÉT, SINH ĐỘNG VÀ DỄ HIỂU NHẤT:
 * 1. TRÁI TIM ĐẬP (BEATING HEART)
 * 2. MẶT CƯỜI NHÁY MẮT (WINKING SMILEY)
 * 3. PAC-MAN HÁ MỒM ĂN KẸO (PAC-MAN RETRO)
 * 4. MÂY MƯA RƠI & SẤM CHỚP (RAIN & LIGHTNING)
 * 5. CỘT SÓNG ÂM NHẠC (AUDIO EQUALIZER PEAK METER)
 * 6. NGỌN LỬA BẬP BÙNG (DOOM FIRE EFFECT)
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "PIXEL_ANIMATION";

// --- CHÂN GPIO ESP32-C3 ---
#define PIN_CLK         GPIO_NUM_1
#define PIN_DIO         GPIO_NUM_0

#define PIN_CS1         GPIO_NUM_20  // IC 1: Cột 0..3
#define PIN_CS2         GPIO_NUM_21  // IC 2: Cột 4..7
#define PIN_CS3         GPIO_NUM_9   // IC 3: Cột 8..11
#define PIN_CS4         GPIO_NUM_10  // IC 4: Cột 12..15

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
    ESP_LOGI(TAG, "IC %d init OK", ic_idx + 1);
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
// HỆ THỐNG ĐỒ HỌA MA TRẬN 16x16 CHUẨN XÁC 100%
// ============================================================================

static void draw_pixel(int x, int y, bool r, bool g, bool b) {
    if (x < 0 || x >= 16 || y < 0 || y >= 16) return;

    uint8_t ic = x / 4;
    uint8_t col = x % 4;
    uint8_t row = 15 - y; // y=0 đỉnh trên cùng, y=15 đáy dưới cùng

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

// ============================================================================
// HIỆU ỨNG 1: TRÁI TIM ĐẬP (BEATING HEART)
// ============================================================================
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
    ESP_LOGI(TAG, ">>> HIEU UNG 1: TRAI TIM DAP (BEATING HEART) <<<");
    int elapsed = 0;
    while (elapsed < duration_ms) {
        // Nhịp 1: Trái tim lớn đỏ rực + điểm bóng sáng trắng
        fb_clear();
        draw_bitmap(HEART_BIG, true, false, false);
        draw_pixel(4, 3, true, true, true); // Điểm bóng sáng
        draw_pixel(4, 4, true, true, true);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(180));

        // Nhịp 2: Trái tim nhỏ
        fb_clear();
        draw_bitmap(HEART_SMALL, true, false, false);
        draw_pixel(6, 4, true, true, true);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(140));

        // Nhịp 3: Trái tim lớn lại
        fb_clear();
        draw_bitmap(HEART_BIG, true, false, false);
        draw_pixel(4, 3, true, true, true);
        draw_pixel(4, 4, true, true, true);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(200));

        // Nghỉ giữa 2 nhịp tim
        fb_clear();
        draw_bitmap(HEART_SMALL, true, false, false);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(450));

        elapsed += (180 + 140 + 200 + 450);
    }
}

// ============================================================================
// HIỆU ỨNG 2: MẶT CƯỜI NHÁY MẮT (WINKING SMILEY)
// ============================================================================
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
        // Mắt mở tươi cười (Màu Vàng)
        fb_clear();
        draw_bitmap(SMILEY_OPEN, true, true, false);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(1200));

        // Nháy mắt
        fb_clear();
        draw_bitmap(SMILEY_WINK, true, true, false);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(350));

        // Mở lại
        fb_clear();
        draw_bitmap(SMILEY_OPEN, true, true, false);
        fb_render();
        vTaskDelay(pdMS_TO_TICKS(800));

        elapsed += (1200 + 350 + 800);
    }
}

// ============================================================================
// HIỆU ỨNG 3: PAC-MAN HÁ MỒM ĐUỔI HẠT NĂNG LƯỢNG
// ============================================================================
static const uint16_t PACMAN_FRAME[3][16] = {
    // Frame 0: Mồm mở to
    { 0x07E0, 0x1FF8, 0x3FFC, 0x7F80, 0x7F00, 0xFE00, 0xFC00, 0xFC00,
      0xFE00, 0x7F00, 0x7F80, 0x3FFC, 0x1FF8, 0x07E0, 0x0000, 0x0000 },
    // Frame 1: Mồm mở vừa
    { 0x07E0, 0x1FF8, 0x3FFC, 0x7FE0, 0x7FC0, 0xFF80, 0xFF00, 0xFF00,
      0xFF80, 0x7FC0, 0x7FE0, 0x3FFC, 0x1FF8, 0x07E0, 0x0000, 0x0000 },
    // Frame 2: Mồm ngậm tròn
    { 0x07E0, 0x1FF8, 0x3FFC, 0x7FFE, 0x7FFE, 0xFFFF, 0xFFFF, 0xFFFF,
      0xFFFF, 0x7FFE, 0x7FFE, 0x3FFC, 0x1FF8, 0x07E0, 0x0000, 0x0000 }
};

static void anim_pacman(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 3: PAC-MAN HA MOM AN KEO <<<");
    int elapsed = 0;
    int f = 0;
    bool dot_blink = true;

    while (elapsed < duration_ms) {
        fb_clear();
        // Vẽ Pac-man màu vàng
        draw_bitmap(PACMAN_FRAME[f % 3], true, true, false);

        // Vẽ 2 hạt kẹo chấm trắng nhấp nháy ở phía trước
        if (dot_blink) {
            draw_pixel(13, 6, true, true, true);
            draw_pixel(13, 7, true, true, true);
            draw_pixel(15, 6, true, true, true);
            draw_pixel(15, 7, true, true, true);
        }

        fb_render();
        vTaskDelay(pdMS_TO_TICKS(140));
        f++;
        if (f % 2 == 0) dot_blink = !dot_blink;
        elapsed += 140;
    }
}

// ============================================================================
// HIỆU ỨNG 4: MÂY MƯA RƠI & SẤM CHỚP (RAIN & LIGHTNING)
// ============================================================================
static const uint16_t CLOUD_BMP[16] = {
    0x0000, 0x0380, 0x0FE0, 0x1FF8, 0x7FFE, 0xFFFF, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
};

typedef struct {
    int x;
    int y;
    int speed;
} RainDrop;

static void anim_rain_and_thunder(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 4: MAY MUA & SAM CHOP <<<");
    RainDrop drops[8];
    for (int i = 0; i < 8; i++) {
        drops[i].x = (i * 2) + (rand() % 2);
        drops[i].y = 6 + (rand() % 8);
        drops[i].speed = 1 + (rand() % 2);
    }

    int elapsed = 0;
    int lightning_timer = 0;

    while (elapsed < duration_ms) {
        fb_clear();

        // 1. Vẽ đám mây màu Xanh Cyan / Trắng
        draw_bitmap(CLOUD_BMP, false, true, true);

        // 2. Cập nhật và vẽ hạt mưa rơi màu Xanh Dương
        for (int i = 0; i < 8; i++) {
            drops[i].y += drops[i].speed;
            if (drops[i].y >= 16) {
                drops[i].y = 6;
                drops[i].x = (i * 2) + (rand() % 2);
            }
            draw_pixel(drops[i].x, drops[i].y, false, false, true);
            // Vệt mưa kéo dài 1 pixel
            if (drops[i].y > 6) {
                draw_pixel(drops[i].x, drops[i].y - 1, false, false, true);
            }
        }

        fb_render();
        vTaskDelay(pdMS_TO_TICKS(80));
        elapsed += 80;
        lightning_timer++;

        // 3. Sấm sét chớp sáng sau mỗi khoảng 2-3 giây
        if (lightning_timer > 30) {
            lightning_timer = 0;
            // Chớp giật tia sét Trắng/Vàng
            for (int flash = 0; flash < 2; flash++) {
                fb_clear();
                // Vẽ tia sét Zigzag màu Vàng
                draw_pixel(8, 5, true, true, false);
                draw_pixel(7, 6, true, true, false);
                draw_pixel(6, 7, true, true, false);
                draw_pixel(7, 8, true, true, true); // Tâm sét trắng
                draw_pixel(8, 8, true, true, true);
                draw_pixel(6, 9, true, true, false);
                draw_pixel(5, 10, true, true, false);
                draw_pixel(4, 11, true, true, false);
                draw_pixel(5, 12, true, true, false);
                draw_pixel(6, 13, true, true, false);
                draw_pixel(7, 14, true, true, false);
                fb_render();
                vTaskDelay(pdMS_TO_TICKS(60));

                // Chớp trắng toàn màn hình
                for (int ic = 0; ic < NUM_ICS; ic++) {
                    for (int r = 0; r < 24; r++) fb[ic][r] = 0xFF;
                }
                fb_render();
                vTaskDelay(pdMS_TO_TICKS(40));

                fb_clear();
                fb_render();
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
    }
}

// ============================================================================
// HIỆU ỨNG 5: CỘT SÓNG ÂM THANH NHẢY NHẠC (AUDIO EQUALIZER)
// ============================================================================
static void anim_equalizer(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 5: COT SONG AM NHAC (EQUALIZER) <<<");
    int heights[16] = {4, 6, 9, 12, 14, 11, 8, 6, 7, 10, 13, 15, 12, 8, 5, 3};
    int peaks[16] = {0};

    int elapsed = 0;
    while (elapsed < duration_ms) {
        fb_clear();

        for (int x = 0; x < 16; x++) {
            // Thay đổi chiều cao ngẫu nhiên tự nhiên theo nhịp điệu
            int delta = (rand() % 5) - 2;
            heights[x] += delta;
            if (heights[x] < 2) heights[x] = 2;
            if (heights[x] > 15) heights[x] = 15;

            // Cập nhật đỉnh rơi chậm (Peak Dot)
            if (heights[x] >= peaks[x]) {
                peaks[x] = heights[x];
            } else {
                if (rand() % 3 == 0) peaks[x]--;
            }

            // Vẽ cột sóng đổi màu từ đáy lên đỉnh:
            // Đáy (y=15..11): Xanh Lá | Giữa (y=10..6): Vàng | Đỉnh (y=5..0): Đỏ
            for (int h = 0; h < heights[x]; h++) {
                int y = 15 - h;
                if (y >= 11) {
                    draw_pixel(x, y, false, true, false); // Xanh Lá
                } else if (y >= 6) {
                    draw_pixel(x, y, true, true, false);  // Vàng
                } else {
                    draw_pixel(x, y, true, false, false); // Đỏ
                }
            }

            // Vẽ điểm đỉnh rơi chậm (Màu Trắng)
            int peak_y = 15 - peaks[x];
            if (peak_y >= 0 && peak_y < 16) {
                draw_pixel(x, peak_y, true, true, true);
            }
        }

        fb_render();
        vTaskDelay(pdMS_TO_TICKS(70)); // ~14 FPS mượt mà
        elapsed += 70;
    }
}

// ============================================================================
// HIỆU ỨNG 6: NGỌN LỬA BẬP BÙNG (DOOM FIRE EFFECT)
// ============================================================================
static void anim_doom_fire(int duration_ms) {
    ESP_LOGI(TAG, ">>> HIEU UNG 6: NGON LUA BAP BUNG (FIRE EFFECT) <<<");
    uint8_t fire[16][16];
    memset(fire, 0, sizeof(fire));

    int elapsed = 0;
    while (elapsed < duration_ms) {
        // 1. Tạo nguồn nhiệt ngẫu nhiên ở đáy (y = 15)
        for (int x = 0; x < 16; x++) {
            fire[15][x] = (rand() % 4 == 0) ? 2 : 3; // Mức 2 (Vàng) hoặc 3 (Trắng)
        }

        // 2. Truyền nhiệt lên trên và làm nguội
        for (int y = 0; y < 15; y++) {
            for (int x = 0; x < 16; x++) {
                int src_x = (x + (rand() % 3) - 1 + 16) % 16;
                int decay = rand() % 2;
                int heat = fire[y + 1][src_x];
                fire[y][x] = (heat > decay) ? (heat - decay) : 0;
            }
        }

        // 3. Render ra màu RGB: 3=Trắng, 2=Vàng, 1=Đỏ, 0=Tắt
        fb_clear();
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                uint8_t h = fire[y][x];
                if (h == 3) {
                    draw_pixel(x, y, true, true, true);   // Trắng (nóng nhất)
                } else if (h == 2) {
                    draw_pixel(x, y, true, true, false);  // Vàng
                } else if (h == 1) {
                    draw_pixel(x, y, true, false, false); // Đỏ
                }
            }
        }

        fb_render();
        vTaskDelay(pdMS_TO_TICKS(50)); // 20 FPS cực mượt
        elapsed += 50;
    }
}

// ============================================================================
// TASK ĐIỀU KHIỂN CHÍNH
// ============================================================================
static void animation_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== KHOI DONG TRINH DIEN DO HOA 16x16 FULL RGB ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

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
    ESP_LOGI(TAG, "ESP32-C3 16x16 RGB PIXEL ART ANIMATIONS");
    init_gpio();
    xTaskCreate(animation_task, "anim_task", 4096, NULL, 5, NULL);
}
