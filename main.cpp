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

// Ghi toàn bộ Display Data RAM của một IC (224 vị trí: 28 ROW x 8 COM)
static void fill_display(uint8_t ic_idx, uint8_t pattern) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(5);
    
    write_byte(CMD_WRITE_DDRAM); // Lệnh ghi RAM (0x80)
    write_byte(0x00);            // Bắt đầu từ địa chỉ 0x00

    // Ghi dữ liệu 224 bytes phủ kín toàn bộ RAM
    for (int i = 0; i < 224; i++) {
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

    // 5. Nạp dữ liệu ban đầu vào toàn bộ DDRAM (0xFF để sáng)
    fill_display(ic_idx, 0xFF);

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

// Task chạy thử nghiệm kiểm tra mạch LED
static void led_test_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== BAT DAU TEST MA TRAN LED 4x HT16D35A ===");

    // Khởi tạo cả 4 IC với dao động nội độc lập và bật toàn bộ COM + Brightness + Max Current
    for (int i = 0; i < NUM_ICS; i++) {
        init_ht16d35a(i);
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    while (1) {
        // Test 1: Sáng lần lượt từng cụm IC để kiểm tra CS1 -> CS4
        for (int i = 0; i < NUM_ICS; i++) {
            ESP_LOGI(TAG, "-> Dang sang IC %d (CS%d)...", i + 1, i + 1);
            fill_display(i, 0xFF); // Bật sáng hết IC này
            vTaskDelay(pdMS_TO_TICKS(1000));
            fill_display(i, 0x00); // Tắt IC này
            vTaskDelay(pdMS_TO_TICKS(200));
        }

        // Test 2: Bật sáng toàn bộ cả 4 cụm IC cùng lúc
        ESP_LOGI(TAG, "-> Sang toan bo ca 4 IC (Max cong suat)...");
        for (int i = 0; i < NUM_ICS; i++) {
            fill_display(i, 0xFF);
        }
        vTaskDelay(pdMS_TO_TICKS(2500));

        // Test 3: Tắt toàn bộ
        ESP_LOGI(TAG, "-> Tat toan bo...");
        for (int i = 0; i < NUM_ICS; i++) {
            fill_display(i, 0x00);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Test 4: Chớp nhấp nháy 3 lần
        for (int k = 0; k < 3; k++) {
            for (int i = 0; i < NUM_ICS; i++) fill_display(i, 0xFF);
            vTaskDelay(pdMS_TO_TICKS(300));
            for (int i = 0; i < NUM_ICS; i++) fill_display(i, 0x00);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Khoi dong he thong ESP32-C3 HT16D35A LED Driver");
    init_gpio();
    xTaskCreate(led_test_task, "led_test_task", 4096, NULL, 5, NULL);
}
