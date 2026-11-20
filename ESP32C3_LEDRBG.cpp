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
 * - CLK          -> GPIO 3 (Chân 10 trên IC, nối chung cả 4 IC)
 * - DIO          -> GPIO 4 (Chân 9 trên IC, nối chung cả 4 IC)
 * - CS1          -> GPIO 5 (Chân 11 của IC 1 - Master)
 * - CS2          -> GPIO 6 (Chân 11 của IC 2 - Slave 1)
 * - CS3          -> GPIO 7 (Chân 11 của IC 3 - Slave 2)
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
#define PIN_CLK         GPIO_NUM_3   // Xung nhịp Clock (Chân 10)
#define PIN_DIO         GPIO_NUM_4   // Dữ liệu Data (Chân 9)

#define PIN_CS1         GPIO_NUM_5   // Chip Select IC 1 (Master)
#define PIN_CS2         GPIO_NUM_6   // Chip Select IC 2 (Slave 1)
#define PIN_CS3         GPIO_NUM_7   // Chip Select IC 3 (Slave 2)
#define PIN_CS4         GPIO_NUM_10  // Chip Select IC 4 (Slave 3)

#define NUM_ICS         4
static const gpio_num_t CS_PINS[NUM_ICS] = {PIN_CS1, PIN_CS2, PIN_CS3, PIN_CS4};

// --- BẢNG MÃ LỆNH CHUẨN DATASHEET HT16D35A ---
#define CMD_SOFT_RESET       0x01
#define CMD_CASCADE_MODE     0x34 // Master / Slave & Clock Source
#define CMD_SYS_CONTROL      0x35 // Oscillator & Display ON/OFF
#define CMD_COM_NUM          0x31 // Cài đặt số COM (COM0..COM7)
#define CMD_BRIGHTNESS       0x38 // Cài đặt độ sáng PWM
#define CMD_BINARY_GRAY      0x30 // Chọn chế độ Binary (0x00) hoặc Gray (0x01)
#define CMD_WRITE_DDRAM      0x80 // Ghi dữ liệu vào RAM hiển thị (DDRAM)

// Hàm trễ microsecond dùng ROM function cho độ chính xác cao
static inline void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

// Hàm gửi 1 byte theo giao thức 3-wire (MSB first, chốt ở cạnh lên CLK)
static void write_byte(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        gpio_set_level(PIN_CLK, 0);
        gpio_set_level(PIN_DIO, (data & (1 << i)) ? 1 : 0);
        delay_us(2);
        gpio_set_level(PIN_CLK, 1);
        delay_us(2);
    }
}

// Gửi 1 lệnh điều khiển (Mã lệnh + Tham số) tới IC chỉ định
static void send_command(uint8_t ic_idx, uint8_t cmd, uint8_t param) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(2);
    write_byte(cmd);
    write_byte(param);
    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(2);
}

// Khởi tạo một IC HT16D35A
static void init_ht16d35a(uint8_t ic_idx, bool is_master) {
    // 1. Reset mềm
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(2);
    write_byte(CMD_SOFT_RESET);
    gpio_set_level(CS_PINS[ic_idx], 1);
    vTaskDelay(pdMS_TO_TICKS(15));

    // 2. Cấu hình Cascade: IC đầu là Master (phát xung SYNC), các IC sau là Slave
    if (is_master) {
        send_command(ic_idx, CMD_CASCADE_MODE, 0x01); // Master: IRC, SYNC output active
        ESP_LOGI(TAG, "IC %d khoi tao: MASTER MODE", ic_idx + 1);
    } else {
        send_command(ic_idx, CMD_CASCADE_MODE, 0x03); // Slave: Nhan xung SYNC
        ESP_LOGI(TAG, "IC %d khoi tao: SLAVE MODE", ic_idx + 1);
    }

    // 3. Cài đặt số COM hoạt động (0x07 = kích hoạt cả COM0 -> COM7)
    send_command(ic_idx, CMD_COM_NUM, 0x07);

    // 4. Chọn chế độ Binary Mode (0x00)
    send_command(ic_idx, CMD_BINARY_GRAY, 0x00);

    // 5. Cài đặt độ sáng tối đa (0x40 = 64/64 PWM)
    send_command(ic_idx, CMD_BRIGHTNESS, 0x40);

    // 6. Bật dao động hệ thống và bật hiển thị LED (0x03 = Osc ON + Display ON)
    send_command(ic_idx, CMD_SYS_CONTROL, 0x03);
}

// Ghi toàn bộ Display Data RAM của một IC (224 vị trí: 28 ROW x 8 COM)
static void fill_display(uint8_t ic_idx, uint8_t pattern) {
    if (ic_idx >= NUM_ICS) return;
    gpio_set_level(CS_PINS[ic_idx], 0);
    delay_us(2);
    
    write_byte(CMD_WRITE_DDRAM); // Lệnh ghi RAM
    write_byte(0x00);            // Bắt đầu từ địa chỉ 0x00

    // Ghi dữ liệu 28 cột x 8 com = 224 bytes
    for (int i = 0; i < 224; i++) {
        write_byte(pattern);
    }

    gpio_set_level(CS_PINS[ic_idx], 1);
    delay_us(2);
}

// Cấu hình các chân GPIO của ESP32-C3
static void init_gpio(void) {
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;

    // Thiết lập chân CLK, DIO
    io_conf.pin_bit_mask = (1ULL << PIN_CLK) | (1ULL << PIN_DIO);
    for (int i = 0; i < NUM_ICS; i++) {
        io_conf.pin_bit_mask |= (1ULL << CS_PINS[i]);
    }
    gpio_config(&io_conf);

    // Mặc định ban đầu
    gpio_set_level(PIN_CLK, 1);
    gpio_set_level(PIN_DIO, 1);
    for (int i = 0; i < NUM_ICS; i++) {
        gpio_set_level(CS_PINS[i], 1); // CS ở mức cao = không chọn chip
    }
}

// Task chạy thử nghiệm kiểm tra mạch LED
static void led_test_task(void *pvParameters) {
    ESP_LOGI(TAG, "=== BAT DAU TEST MA TRAN LED 4x HT16D35A ===");

    // Khởi tạo các IC: IC 0 là Master, IC 1..3 là Slave
    init_ht16d35a(0, true);
    for (int i = 1; i < NUM_ICS; i++) {
        init_ht16d35a(i, false);
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
