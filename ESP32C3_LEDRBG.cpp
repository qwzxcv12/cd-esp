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

// --- BẢNG MÃ LỆNH CHUẨN DATASHEET HT16D35A ---
#define CMD_SOFT_RESET       0x00 // Software Reset (1 byte)
#define CMD_BINARY_GRAY      0x31 // Binary Mode (0x01) hoặc Gray Mode (0x00)
#define CMD_COM_NUM          0x32 // Cài đặt số COM (0x07 = kích hoạt COM0..COM7)
#define CMD_CASCADE_MODE     0x34 // Master / Slave & Clock Source (0x01 = IRC Master)
#define CMD_SYS_CONTROL      0x35 // Oscillator & Display ON/OFF (0x03 = Osc ON + Display ON)
#define CMD_CURRENT_RATIO    0x36 // Constant Current Ratio (0x07 = Max current 8/8)
#define CMD_BRIGHTNESS       0x37 // Cài đặt độ sáng PWM (0x3F = 63 max PWM)
#define CMD_FUNCTION_ENABLE  0x38 // Function Enable (0x00 = Fade/Blink/Mask OFF)
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
        delay_us(4);
        gpio_set_level(PIN_CLK, 1);
        delay_us(4);
    }
    gpio_set_level(PIN_CLK, 0);
    delay_us(2);
}

// Gửi 1 lệnh điều khiển (Mã lệnh + Tham số) tới IC chỉ định
static void send_command(uint8_t ic_idx, uint8_t cmd, uint8_t param) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(5);
    write_byte(cmd);
    write_byte(param);
    delay_us(5);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(5);
}

// Khởi tạo một IC HT16D35A
static void init_ht16d35a(uint8_t ic_idx) {
    // 1. Reset mềm IC (Lệnh 0x00)
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(5);
    write_byte(CMD_SOFT_RESET);
    delay_us(5);
    gpio_set_level(CS_PINS[ic_idx], 1);
    vTaskDelay(pdMS_TO_TICKS(20));

    // 2. Chế độ dao động nội IRC (0x01) để tất cả IC đều tự phát xung hoạt động độc lập
    send_command(ic_idx, CMD_CASCADE_MODE, 0x01);

    // 3. Bật bộ dao động hệ thống và bật hiển thị LED (0x03 = Osc ON + Display ON)
    send_command(ic_idx, CMD_SYS_CONTROL, 0x03);

    // 4. Kích hoạt toàn bộ COM0 -> COM7 (0x07 = 8 COM pins active)
    send_command(ic_idx, CMD_COM_NUM, 0x07);

    // 5. Cấu hình dòng cực đại cho ROW (Level 7 = 8/8 full current sink)
    send_command(ic_idx, CMD_CURRENT_RATIO, 0x07);

    // 6. Cài đặt độ sáng PWM tối đa (0x3F = 63/63)
    send_command(ic_idx, CMD_BRIGHTNESS, 0x3F);

    // 7. Chọn chế độ Binary Mode (0x01)
    send_command(ic_idx, CMD_BINARY_GRAY, 0x01);

    // 8. Tắt chức năng Mask / Blink / Fade để màn hình hiển thị trực tiếp
    send_command(ic_idx, CMD_FUNCTION_ENABLE, 0x00);

    ESP_LOGI(TAG, "IC %d: Da khoi tao thanh cong voi day du lenh chuan Datasheet", ic_idx + 1);
}

// Ghi toàn bộ Display Data RAM của một IC (224 vị trí: 28 ROW x 8 COM)
static void fill_display(uint8_t ic_idx, uint8_t pattern) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(5);
    
    write_byte(CMD_WRITE_DDRAM); // Lệnh ghi RAM (0x80)
    write_byte(0x00);            // Bắt đầu từ địa chỉ 0x00

    // Ghi dữ liệu 224 bytes phủ kín toàn bộ RAM (hỗ trợ cả Binary Mode lẫn Gray Mode)
    for (int i = 0; i < 224; i++) {
        write_byte(pattern);
    }

    delay_us(5);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(5);
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
