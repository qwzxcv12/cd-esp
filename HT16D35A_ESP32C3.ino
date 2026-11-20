/**
 * ============================================================================
 * DỰ ÁN ARDUINO: ĐIỀU KHIỂN MA TRẬN LED 4x HT16D35A (ESP32 / ESP32-C3)
 * File: HT16D35A_ESP32C3.ino
 * ============================================================================
 * 
 * SƠ ĐỒ ĐẤU NỐI:
 * - GND          -> GND ESP32-C3 & Nguồn 5V
 * - VDD (Logic)  -> 3.3V ESP32-C3
 * - LED_VDD      -> 5V nguồn ngoài (Chân 14, 21 trên IC)
 * - CLK          -> GPIO 1 (Chân 10 trên IC, nối chung 4 IC)
 * - DIO          -> GPIO 0 (Chân 9 trên IC, nối chung 4 IC)
 * - CS1          -> GPIO 20 (Chân 11 của IC 1)
 * - CS2          -> GPIO 21 (Chân 11 của IC 2)
 * - CS3          -> GPIO 9 (Chân 11 của IC 3)
 * - CS4          -> GPIO 10 (Chân 11 của IC 4)
 * ============================================================================
 */

#include <Arduino.h>

// Định nghĩa chân ESP32-C3
#define PIN_CLK   1   // Xung Clock (Chân 10)
#define PIN_DIO   0   // Dữ liệu Data (Chân 9)

const int CS_PINS[4] = {20, 21, 9, 10};

// Bảng lệnh Holtek HT16D35A
#define CMD_SOFT_RESET       0x01
#define CMD_CASCADE_MODE     0x34
#define CMD_SYS_CONTROL      0x35
#define CMD_COM_NUM          0x31
#define CMD_BRIGHTNESS       0x38
#define CMD_BINARY_GRAY      0x30
#define CMD_WRITE_DDRAM      0x80

void writeByte(uint8_t data) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_DIO, (data & (1 << i)) ? HIGH : LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_CLK, HIGH);
    delayMicroseconds(2);
  }
}

void sendCommand(uint8_t icIndex, uint8_t cmd, uint8_t param) {
  digitalWrite(CS_PINS[icIndex], LOW);
  delayMicroseconds(2);
  writeByte(cmd);
  writeByte(param);
  digitalWrite(CS_PINS[icIndex], HIGH);
  delayMicroseconds(2);
}

void initHT16D35A(uint8_t icIndex, bool isMaster) {
  digitalWrite(CS_PINS[icIndex], LOW);
  writeByte(CMD_SOFT_RESET);
  digitalWrite(CS_PINS[icIndex], HIGH);
  delay(15);

  if (isMaster) {
    sendCommand(icIndex, CMD_CASCADE_MODE, 0x01); // Master Mode
  } else {
    sendCommand(icIndex, CMD_CASCADE_MODE, 0x03); // Slave Mode
  }

  sendCommand(icIndex, CMD_COM_NUM, 0x07);     // Kích hoạt COM0..COM7
  sendCommand(icIndex, CMD_BINARY_GRAY, 0x00); // Binary Mode
  sendCommand(icIndex, CMD_BRIGHTNESS, 0x40);  // Độ sáng tối đa
  sendCommand(icIndex, CMD_SYS_CONTROL, 0x03); // Bật OSC & Display
}

void fillDisplay(uint8_t icIndex, uint8_t pattern) {
  digitalWrite(CS_PINS[icIndex], LOW);
  delayMicroseconds(2);
  writeByte(CMD_WRITE_DDRAM);
  writeByte(0x00);
  for (int i = 0; i < 224; i++) {
    writeByte(pattern);
  }
  digitalWrite(CS_PINS[icIndex], HIGH);
  delayMicroseconds(2);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_DIO, OUTPUT);
  digitalWrite(PIN_CLK, HIGH);
  digitalWrite(PIN_DIO, HIGH);

  for (int i = 0; i < 4; i++) {
    pinMode(CS_PINS[i], OUTPUT);
    digitalWrite(CS_PINS[i], HIGH);
  }

  Serial.println("\n--- Khoi tao HT16D35A (4 ICs) ---");
  initHT16D35A(0, true);  // IC 1 Master
  for (int i = 1; i < 4; i++) {
    initHT16D35A(i, false); // IC 2..4 Slave
  }
  Serial.println("Khoi tao thanh cong!");
}

void loop() {
  // 1. Sáng tuần tự từng IC
  for (int i = 0; i < 4; i++) {
    Serial.printf("Sang IC %d\n", i + 1);
    fillDisplay(i, 0xFF);
    delay(1000);
    fillDisplay(i, 0x00);
    delay(200);
  }

  // 2. Sáng cả 4 IC
  Serial.println("Sang toan bo 4 IC");
  for (int i = 0; i < 4; i++) fillDisplay(i, 0xFF);
  delay(2000);

  // 3. Tắt cả 4 IC
  for (int i = 0; i < 4; i++) fillDisplay(i, 0x00);
  delay(1000);
}
