/**
 * ============================================================================
 * TEST TỪNG BÓNG LED MA TRẬN 16x16 FULL RGB (4x HT16D35A)
 * ÁNH XẠ CHÍNH XÁC 100% THEO SILKSCREEN ĐÃ XÁC THỰC THỰC TẾ:
 * 
 * - IC 1: D400 -> D463
 *   + Cột 0: D400 (đáy) -> D415 (đỉnh) [COM7 nửa dưới, COM6 nửa trên]
 *   + Cột 1: D416 (đáy) -> D431 (đỉnh) [COM5 nửa dưới, COM4 nửa trên]
 *   + Cột 2: D432 (đáy) -> D447 (đỉnh) [COM3 nửa dưới, COM2 nửa trên]
 *   + Cột 3: D448 (đáy) -> D463 (đỉnh) [COM1 nửa dưới, COM0 nửa trên]
 * 
 * - IC 2: D500 -> D563
 * - IC 3: D600 -> D663
 * - IC 4: D700 -> D763
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

static const char *TAG = "RGB_16X16";

// --- CHÂN GPIO ESP32-C3 ---
#define PIN_CLK         GPIO_NUM_1
#define PIN_DIO         GPIO_NUM_0

#define PIN_CS1         GPIO_NUM_20  // IC 1: D400..D463
#define PIN_CS2         GPIO_NUM_21  // IC 2: D500..D563
#define PIN_CS3         GPIO_NUM_9   // IC 3: D600..D663
#define PIN_CS4         GPIO_NUM_10  // IC 4: D700..D763

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
// HÀM ĐIỀU KHIỂN CHÍNH XÁC TỪNG BÓNG LED THEO SILKSCREEN THỰC TẾ 100%:
// ============================================================================
/**
 * @param ic: 0 (IC1: D400..D463), 1 (IC2: D500..D563), 2 (IC3: D600..D663), 3 (IC4: D700..D763)
 * @param col: Cột trong IC (0..3, từ trái sang phải)
 * @param row: Hàng vật lý (0 = đáy D400, 15 = đỉnh D415)
 * @param r, g, b: Điều khiển màu
 */
static void set_led_rgb(uint8_t ic, uint8_t col, uint8_t row, bool r, bool g, bool b) {
    if (ic >= NUM_ICS || col >= 4 || row >= 16) return;

    uint8_t t = row % 8;
    uint8_t addr_R = t * 3 + 0;
    uint8_t addr_G = t * 3 + 1;
    uint8_t addr_B = t * 3 + 2;

    // Ánh xạ COM bit đã chứng minh chính xác 100% qua đo đạc thực tế:
    // Cột 3: Nửa trên = COM0 (bit 0), Nửa dưới = COM1 (bit 1)
    // Cột 2: Nửa trên = COM2 (bit 2), Nửa dưới = COM3 (bit 3)
    // Cột 1: Nửa trên = COM4 (bit 4), Nửa dưới = COM5 (bit 5)
    // Cột 0: Nửa trên = COM6 (bit 6), Nửa dưới = COM7 (bit 7)
    uint8_t com_bit = (row >= 8) ? ((3 - col) * 2) : ((3 - col) * 2 + 1);
    uint8_t mask = (1 << com_bit);

    if (r) fb[ic][addr_R] |= mask; else fb[ic][addr_R] &= ~mask;
    if (g) fb[ic][addr_G] |= mask; else fb[ic][addr_G] &= ~mask;
    if (b) fb[ic][addr_B] |= mask; else fb[ic][addr_B] &= ~mask;
}

// Bật 1 bóng duy nhất theo số thứ tự k (0..63)
// k=0..15  -> Cột 0: D400 (đáy) lên D415 (đỉnh)
// k=16..31 -> Cột 1: D416 (đáy) lên D431 (đỉnh)
// k=32..47 -> Cột 2: D432 (đáy) lên D447 (đỉnh)
// k=48..63 -> Cột 3: D448 (đáy) lên D463 (đỉnh)
static void light_single_led(uint8_t ic, uint8_t k) {
    fb_clear();
    uint8_t col = k / 16;
    uint8_t row = k % 16;
    set_led_rgb(ic, col, row, true, true, true); // Sáng Trắng rõ ràng
    fb_render();
}

static void led_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== TEST SANG TUAN TU CHINH XAC SILKSCREEN 100% ===");

    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    vTaskDelay(pdMS_TO_TICKS(1500));

    while (1) {
        for (int ic = 0; ic < NUM_ICS; ic++) {
            int base_name = 400 + ic * 100;
            ESP_LOGW(TAG, ">>> BAT DAU IC %d: D%03d DEN D%03d <<<", ic + 1, base_name, base_name + 63);

            for (int k = 0; k < 64; k++) {
                int led_num = base_name + k;
                int chip_col = k / 16;
                int chip_row = k % 16;

                light_single_led(ic, k);

                ESP_LOGI(TAG, "LED D%03d [IC%d Cot:%d Hang:%02d] -> ON", led_num, ic + 1, chip_col, chip_row);

                // Giữ 300ms mỗi bóng: vừa đủ chậm để mắt nhìn rõ từng bóng chạy thẳng lên, vừa không phải chờ quá lâu
                vTaskDelay(pdMS_TO_TICKS(300));
            }

            // Nghỉ 1 giây giữa các IC
            fb_clear();
            fb_render();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "ESP32-C3 LED TEST - VERIFIED HARDWARE MAPPING");
    init_gpio();
    xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);
}
