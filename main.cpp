/**
 * ============================================================================
 * HỆ THỐNG TỰ ĐỘNG DÒ CHÂN IC PI4IOE5V6416 & 4x CHIP LED HT16D35A
 * Tương thích ESP-IDF v5.1 (ESP32-C3)
 * ----------------------------------------------------------------------------
 * CẤU HÌNH PHẦN CỨNG THEO ĐO THỰC TẾ CỦA BẠN:
 * - Chân 19 (SCL IC vuông) ---> GPIO 21 của ESP32-C3
 * - Chân 20 (SDA IC vuông) ---> GPIO 20 của ESP32-C3
 * - Chân 9  (Que dò)       ---> GPIO 9  của ESP32-C3 (Chạm vào chân 11 của HT16D35A)
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

// Cấu hình chân phần cứng chuẩn xác của bạn
#define I2C_SCL_PIN         GPIO_NUM_21  // Nối vào Chân 19 của IC
#define I2C_SDA_PIN         GPIO_NUM_20  // Nối vào Chân 20 của IC
#define PROBE_PIN           GPIO_NUM_9   // Chân que dò chạm vào các chân CS

#define I2C_PORT            I2C_NUM_0
#define I2C_FREQ_HZ         100000

// Bảng tra cứu số thứ tự bit (0..15) ra số chân vật lý trên IC vuông TQFN-24
static const uint8_t TQFN_PINS[16] = {
    1,  2,  3,  4,  5,  6,  7,  8,   // P0_0 đến P0_7 (Port 0)
    10, 11, 12, 13, 14, 15, 16, 17  // P1_0 đến P1_7 (Port 1)
};

static uint8_t i2c_addr = 0x20;

static esp_err_t i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};
    return i2c_master_write_to_device(I2C_PORT, addr, data, 2, pdMS_TO_TICKS(50));
}

static bool test_i2c_connection(uint8_t addr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

extern "C" void app_main(void) {
    vTaskDelay(pdMS_TO_TICKS(1500)); // Chờ USB Serial/JTAG sẵn sàng

    printf("\n\n============================================================\n");
    printf("   HỆ THỐNG TỰ ĐỘNG DÒ CHÂN IC PI4IOE5V6416 & 4x HT16D35A\n");
    printf("============================================================\n");
    printf("Cấu hình kết nối:\n");
    printf(" - SCL (Chân 19 IC) <---> GPIO 21 ESP32-C3\n");
    printf(" - SDA (Chân 20 IC) <---> GPIO 20 ESP32-C3\n");
    printf(" - DÂY QUE DÒ       <---> GPIO 9  ESP32-C3 (Chạm vào chân CSB)\n");
    printf("============================================================\n\n");

    // 1. Cấu hình chân que dò (GPIO 9) làm INPUT với PULL-UP
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL << PROBE_PIN);
    gpio_config(&io_conf);

    // 2. Khởi tạo bus I2C trên GPIO 20 (SDA) và GPIO 21 (SCL)
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA_PIN;
    conf.scl_io_num = I2C_SCL_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FREQ_HZ;

    i2c_param_config(I2C_PORT, &conf);
    esp_err_t err = i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        printf("[LỖI] Không thể khởi tạo I2C driver (Mã lỗi: %d)\n", err);
        return;
    }

    // 3. Quét kiểm tra chip PI4IOE5V6416 tại địa chỉ 0x20 hoặc 0x21
    printf("[*] Đang kiểm tra kết nối với IC qua I2C...\n");
    if (test_i2c_connection(0x20)) {
        i2c_addr = 0x20;
        printf("✅ ĐÃ TÌM THẤY CHIP PI4IOE5V6416 TẠI ĐỊA CHỈ: 0x20!\n");
    } else if (test_i2c_connection(0x21)) {
        i2c_addr = 0x21;
        printf("✅ ĐÃ TÌM THẤY CHIP PI4IOE5V6416 TẠI ĐỊA CHỈ: 0x21!\n");
    } else {
        printf("⚠️ CẢNH BÁO: Chưa nhận diện được IC tại địa chỉ 0x20 hoặc 0x21!\n");
        printf("-> Vui lòng kiểm tra lại nguồn 3.3V, Mass GND và 2 dây SCL/SDA.\n");
    }

    // 4. Cấu hình các thanh ghi cho PI4IOE5V6416
    // Xuất mức HIGH ban đầu cho cả Port 0 và Port 1
    i2c_write_reg(i2c_addr, 0x02, 0xFF);
    i2c_write_reg(i2c_addr, 0x03, 0xFF);

    // Đặt toàn bộ 16 chân làm OUTPUT (0 = Output)
    i2c_write_reg(i2c_addr, 0x06, 0x00);
    i2c_write_reg(i2c_addr, 0x07, 0x00);

    // Cấu hình chế độ ngõ ra Open-Drain (Thanh ghi 0x4F: 0x03) để an toàn tuyệt đối
    i2c_write_reg(i2c_addr, 0x4F, 0x03);

    printf("\n👉 BẮT ĐẦU QUÉT:\n");
    printf("Hãy cầm que dò (dây nối từ GPIO 9) chạm vào Chân 11 của từng con IC LED HT16D35A...\n\n");

    int last_found_pin = -1;
    uint32_t count = 0;

    while (1) {
        bool match_in_this_cycle = false;

        // Quét lần lượt qua 16 chân của PI4IOE5V6416
        for (int pin = 0; pin < 16; pin++) {
            // Kéo duy nhất chân 'pin' xuống mức LOW (0)
            uint16_t mask = ~(1 << pin);
            i2c_write_reg(i2c_addr, 0x02, mask & 0xFF);
            i2c_write_reg(i2c_addr, 0x03, (mask >> 8) & 0xFF);

            esp_rom_delay_us(100); // Chờ điện áp ổn định

            // Đọc que dò GPIO 9 xem có bị kéo xuống mức LOW không
            if (gpio_get_level(PROBE_PIN) == 0) {
                match_in_this_cycle = true;
                if (pin != last_found_pin) {
                    last_found_pin = pin;

                    printf("\n************************************************************\n");
                    printf("🎯 [PHÁT HIỆN TÍN HIỆU THÔNG MẠCH!]\n");
                    printf("-> Chân que dò (GPIO 9) đang chạm vào đường nối với:\n");
                    printf("   + Tên logic IC      : P%d_%d (Bit số %d)\n", pin / 8, pin % 8, pin);
                    printf("   + CHÂN VẬT LÝ IC VUÔNG : CHÂN SỐ %d (TQFN-24)\n", TQFN_PINS[pin]);
                    printf("************************************************************\n\n");
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
            }

            // Đưa chân 'pin' về lại mức HIGH
            i2c_write_reg(i2c_addr, 0x02, 0xFF);
            i2c_write_reg(i2c_addr, 0x03, 0xFF);
        }

        if (!match_in_this_cycle) {
            last_found_pin = -1;
        }

        count++;
        if (count % 100 == 0) {
            printf("[Trạng thái] Hệ thống đang chạy... Hãy chạm GPIO 9 vào chân 11 IC LED.\n");
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
