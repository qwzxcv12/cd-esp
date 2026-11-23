/**
 * ============================================================================
 * HỆ THỐNG TỰ ĐỘNG DÒ CHÂN IC PI4IOE5V6416 & MẠCH 4x HT16D35A
 * Tương thích ESP-IDF v5.1 (ESP32-C3)
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

static const char *TAG = "PIN_SCANNER";

#define I2C_NUM             I2C_NUM_0
#define I2C_FREQ_HZ         100000

// Bảng ánh xạ từ số thứ tự I/O (0..15) ra số chân vật lý
// Đối với TQFN-24:
static const uint8_t TQFN_PINS[16] = {
    1,  2,  3,  4,  5,  6,  7,  8,   // P0_0 đến P0_7
    10, 11, 12, 13, 14, 15, 16, 17  // P1_0 đến P1_7
};

// Đối với TSSOP-24:
static const uint8_t TSSOP_PINS[16] = {
    4,  5,  6,  7,  8,  9,  10, 11,  // P0_0 đến P0_7
    13, 14, 15, 16, 17, 18, 19, 20  // P1_0 đến P1_7
};

// Danh sách các cặp chân GPIO dự phòng để quét I2C tự động nếu chưa biết chân
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
    {GPIO_NUM_2, GPIO_NUM_3},
    {GPIO_NUM_3, GPIO_NUM_2}
};
#define NUM_CANDIDATE_PAIRS (sizeof(CANDIDATE_PAIRS) / sizeof(CANDIDATE_PAIRS[0]))

// Các chân GPIO dùng làm dây dò (Probe) trên ESP32-C3
static const gpio_num_t PROBE_PINS[] = {
    GPIO_NUM_2,   // Chân que dò chính (Khuyến nghị dùng chân này)
    GPIO_NUM_3,   // Chân que dò phụ
    GPIO_NUM_6,
    GPIO_NUM_7,
    GPIO_NUM_8,
    GPIO_NUM_10,
    GPIO_NUM_20,
    GPIO_NUM_21
};
#define NUM_PROBE_PINS (sizeof(PROBE_PINS) / sizeof(PROBE_PINS[0]))

static uint8_t found_i2c_addr = 0x20;
static gpio_num_t active_sda = GPIO_NUM_4;
static gpio_num_t active_scl = GPIO_NUM_5;

static esp_err_t i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    return i2c_master_write_to_device(I2C_NUM, addr, data, 2, pdMS_TO_TICKS(50));
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

static bool auto_detect_i2c_bus(void) {
    ESP_LOGI(TAG, "Đang tự động quét tìm bus I2C kết nối với PI4IOE5V6416...");
    
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
        if (i2c_driver_install(I2C_NUM, conf.mode, 0, 0, 0) != ESP_OK) {
            continue;
        }

        // Kiểm tra địa chỉ 0x20 và 0x21
        if (test_i2c_connection(0x20)) {
            found_i2c_addr = 0x20;
            active_sda = sda;
            active_scl = scl;
            ESP_LOGI(TAG, ">>> THÀNH CÔNG! Tìm thấy PI4IOE5V6416 tại địa chỉ 0x20: SDA=GPIO %d, SCL=GPIO %d <<<", sda, scl);
            return true;
        }
        if (test_i2c_connection(0x21)) {
            found_i2c_addr = 0x21;
            active_sda = sda;
            active_scl = scl;
            ESP_LOGI(TAG, ">>> THÀNH CÔNG! Tìm thấy PI4IOE5V6416 tại địa chỉ 0x21: SDA=GPIO %d, SCL=GPIO %d <<<", sda, scl);
            return true;
        }

        i2c_driver_delete(I2C_NUM);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Nếu không tự nhận, dùng cấu hình mặc định GPIO 4 & 5
    ESP_LOGW(TAG, "Không tự phát hiện được, thiết lập mặc định: SDA=GPIO 4, SCL=GPIO 5");
    active_sda = GPIO_NUM_4;
    active_scl = GPIO_NUM_5;
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = active_sda;
    conf.scl_io_num = active_scl;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FREQ_HZ;
    i2c_param_config(I2C_NUM, &conf);
    i2c_driver_install(I2C_NUM, conf.mode, 0, 0, 0);
    return false;
}

static void setup_probe_pins(void) {
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    uint64_t pin_mask = 0;
    for (size_t i = 0; i < NUM_PROBE_PINS; i++) {
        // Bỏ qua nếu chân đó đang dùng cho I2C
        if (PROBE_PINS[i] == active_sda || PROBE_PINS[i] == active_scl) continue;
        pin_mask |= (1ULL << PROBE_PINS[i]);
    }
    io_conf.pin_bit_mask = pin_mask;
    gpio_config(&io_conf);
}

extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1500)); // Chờ USB Serial ổn định

    printf("\n\n============================================================\n");
    printf("   HỆ THỐNG TỰ ĐỘNG DÒ CHÂN IC PI4IOE5V6416 & 4x HT16D35A\n");
    printf("============================================================\n");

    // 1. Dò bus I2C
    auto_detect_i2c_bus();

    // 2. Cấu hình các chân Que dò trên ESP32
    setup_probe_pins();

    // 3. Khởi tạo chip mở rộng PI4IOE5V6416
    // Output Port 0 & 1 ban đầu đặt mức HIGH (1)
    i2c_write_reg(found_i2c_addr, 0x02, 0xFF);
    i2c_write_reg(found_i2c_addr, 0x03, 0xFF);

    // Cấu hình toàn bộ 16 chân làm OUTPUT (0 = Output)
    i2c_write_reg(found_i2c_addr, 0x06, 0x00);
    i2c_write_reg(found_i2c_addr, 0x07, 0x00);

    // Cấu hình chế độ ngõ ra Open-Drain (Thanh ghi 0x4F: 0x03) để an toàn tuyệt đối
    i2c_write_reg(found_i2c_addr, 0x4F, 0x03);

    printf("\n[HƯỚNG DẪN SỬ DỤNG QUE DÒ]:\n");
    printf("1. Lấy 1 sợi dây nối vào GPIO 2 (hoặc GPIO 3) của ESP32 làm QUE DÒ.\n");
    printf("2. Cầm đầu dây que dò chạm vào Chân 11 (CS) của từng chip LED HT16D35A.\n");
    printf("3. Màn hình bên dưới sẽ lập tức báo chân tương ứng của IC vuông!\n\n");
    printf("--- HỆ THỐNG ĐANG QUÉT LIÊN TỤC ---\n\n");

    int last_reported_pin = -1;
    gpio_num_t last_reported_gpio = GPIO_NUM_NC;
    uint32_t heartbeat_tick = 0;

    while (1) {
        bool detected_in_cycle = false;

        // Quét lần lượt 16 chân của PI4IOE5V6416
        for (int pin = 0; pin < 16; pin++) {
            // Kéo duy nhất chân 'pin' xuống mức LOW (0)
            uint16_t mask = ~(1 << pin);
            i2c_write_reg(found_i2c_addr, 0x02, mask & 0xFF);
            i2c_write_reg(found_i2c_addr, 0x03, (mask >> 8) & 0xFF);

            esp_rom_delay_us(100); // Chờ ổn định

            // Đọc kiểm tra các chân Que dò trên ESP32
            for (size_t i = 0; i < NUM_PROBE_PINS; i++) {
                gpio_num_t g = PROBE_PINS[i];
                if (g == active_sda || g == active_scl) continue;

                if (gpio_get_level(g) == 0) {
                    detected_in_cycle = true;
                    if (pin != last_reported_pin || g != last_reported_gpio) {
                        last_reported_pin = pin;
                        last_reported_gpio = g;

                        printf("\n************************************************************\n");
                        printf("🎯 PHÁT HIỆN TÍN HIỆU THÔNG MẠCH!\n");
                        printf("-> Que dò (GPIO %d) đang chạm vào chân:\n", g);
                        printf("   + Tên logic IC : P%d_%d (Bit %d)\n", pin / 8, pin % 8, pin);
                        printf("   + IC VUÔNG TQFN-24: CHÂN SỐ %d\n", TQFN_PINS[pin]);
                        printf("   + IC RẾT TSSOP-24 : CHÂN SỐ %d\n", TSSOP_PINS[pin]);
                        printf("************************************************************\n\n");
                    }
                }
            }

            // Đưa chân 'pin' về lại mức HIGH
            i2c_write_reg(found_i2c_addr, 0x02, 0xFF);
            i2c_write_reg(found_i2c_addr, 0x03, 0xFF);
        }

        if (!detected_in_cycle) {
            last_reported_pin = -1;
            last_reported_gpio = GPIO_NUM_NC;
        }

        heartbeat_tick++;
        if (heartbeat_tick % 100 == 0) { // Cứ khoảng 5 giây in 1 dòng trạng thái
            printf("[Trạng thái] Đang lắng nghe que dò... (SDA: GPIO %d, SCL: GPIO %d)\n", active_sda, active_scl);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
