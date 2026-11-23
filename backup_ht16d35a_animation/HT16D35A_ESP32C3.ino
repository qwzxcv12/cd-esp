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

// Bảng lệnh Holtek HT16D35A chuẩn Pimoroni / Holtek
#define CMD_SOFT_RESET       0xCC
#define CMD_SOFT_RESET_ALT   0x00
#define CMD_GLOBAL_BRIGHTNESS 0x37
#define CMD_SCROLL_CTRL      0x20
#define CMD_COM_PIN_CTRL     0x41
#define CMD_ROW_PIN_CTRL     0x42
#define CMD_SYSTEM_CTRL      0x35
#define CMD_WRITE_DDRAM      0x80

void writeByte(uint8_t data) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_DIO, (data & (1 << i)) ? HIGH : LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_CLK, HIGH);
    delayMicroseconds(2);
  }
  digitalWrite(PIN_CLK, LOW);
  delayMicroseconds(1);
}

void sendCmd1b(uint8_t icIndex, uint8_t cmd) {
  digitalWrite(CS_PINS[icIndex], LOW);
  delayMicroseconds(5);
  writeByte(cmd);
  delayMicroseconds(5);
  digitalWrite(CS_PINS[icIndex], HIGH);
  delayMicroseconds(5);
}

void sendCmd2b(uint8_t icIndex, uint8_t cmd, uint8_t p1) {
  digitalWrite(CS_PINS[icIndex], LOW);
  delayMicroseconds(5);
  writeByte(cmd);
  writeByte(p1);
  delayMicroseconds(5);
  digitalWrite(CS_PINS[icIndex], HIGH);
  delayMicroseconds(5);
}

void sendCmd5b(uint8_t icIndex, uint8_t cmd, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4) {
  digitalWrite(CS_PINS[icIndex], LOW);
  delayMicroseconds(5);
  writeByte(cmd);
  writeByte(p1);
  writeByte(p2);
  writeByte(p3);
  writeByte(p4);
  delayMicroseconds(5);
  digitalWrite(CS_PINS[icIndex], HIGH);
  delayMicroseconds(5);
}

void fillDisplay(uint8_t icIndex, uint8_t pattern) {
  digitalWrite(CS_PINS[icIndex], LOW);
  delayMicroseconds(5);
  writeByte(CMD_WRITE_DDRAM);
  writeByte(0x00);
  for (int i = 0; i < 224; i++) {
    writeByte(pattern);
  }
  delayMicroseconds(5);
  digitalWrite(CS_PINS[icIndex], HIGH);
  delayMicroseconds(5);
}

void initHT16D35A(uint8_t icIndex) {
  sendCmd1b(icIndex, CMD_SOFT_RESET);
  delay(10);
  sendCmd1b(icIndex, CMD_SOFT_RESET_ALT);
  delay(10);

  sendCmd2b(icIndex, CMD_GLOBAL_BRIGHTNESS, 0x3F);
  sendCmd2b(icIndex, CMD_SCROLL_CTRL, 0x00);
  sendCmd2b(icIndex, CMD_SYSTEM_CTRL, 0x00);

  fillDisplay(icIndex, 0xFF);

  sendCmd2b(icIndex, CMD_COM_PIN_CTRL, 0xFF);
  sendCmd5b(icIndex, CMD_ROW_PIN_CTRL, 0xFF, 0xFF, 0xFF, 0xFF);
  sendCmd2b(icIndex, CMD_SYSTEM_CTRL, 0x03);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_CLK, OUTPUT);
  pinMode(PIN_DIO, OUTPUT);
  digitalWrite(PIN_CLK, LOW);
  digitalWrite(PIN_DIO, LOW);

  for (int i = 0; i < 4; i++) {
    pinMode(CS_PINS[i], OUTPUT);
    digitalWrite(CS_PINS[i], HIGH);
  }

  Serial.println("\n--- Khoi tao HT16D35A (4 ICs) ---");
  for (int i = 0; i < 4; i++) {
    initHT16D35A(i);
  }
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
