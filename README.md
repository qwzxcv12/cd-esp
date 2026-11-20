# DỰ ÁN ĐIỀU KHIỂN MẠCH MA TRẬN LED 4x HT16D35A VỚI ESP32-C3

Thư mục lưu trữ: `D:\thien\01_DuAn_Code\espc3 - led`

---

## 1. TỔNG QUAN HỆ THỐNG
Mạch gồm **4 chip HT16D35A (Holtek 48-LQFP)** ghép tầng (Cascade) để điều khiển ma trận LED công suất lớn:
* **Giao tiếp:** SPI 3-dây (3-Wire Serial: `CLK`, `DIO`, `CSB`).
* **Kiến trúc ghép 4 chip:**
  * Đường xung nhịp **CLK (Chân 10)** và đường dữ liệu **DIO (Chân 9)** được nối song song chung cho cả 4 chip.
  * Đường chọn chip **CSB (Chân 11)** được tách riêng độc lập cho từng chip (`CS1`, `CS2`, `CS3`, `CS4`).
  * Đường đồng bộ **SYNC (Chân 12)** đã được bo mạch nối sẵn với nhau (IC 1 đóng vai trò **Master**, IC 2, 3, 4 đóng vai trò **Slave**).

---

## 2. BẢNG TRA CHÂN IC HT16D35A (48 LQFP-EP-A)

| Chân số | Tên chân | Chức năng | Kết nối thực tế |
| :---: | :--- | :--- | :--- |
| **6** | `VSS` | Mass logic | Nối GND chung |
| **7** | `VDD` | Nguồn logic (2.7V - 5.5V) | Cấp 3.3V từ ESP32 (hoặc 5V ngoài) |
| **9** | `DIO` | Đường dữ liệu (Data In/Out) | Nối chung cả 4 IC $\rightarrow$ Nối vào **GPIO 4** ESP32-C3 |
| **10** | `CLK` | Xung nhịp đồng hồ (Clock) | Nối chung cả 4 IC $\rightarrow$ Nối vào **GPIO 3** ESP32-C3 |
| **11** | `CSB` | Chip Select (Active LOW) | **Độc lập** từng IC $\rightarrow$ Nối **GPIO 5, 6, 7, 10** ESP32-C3 |
| **12** | `SYNC`| Xung đồng bộ hiển thị | Nối thông giữa 4 IC trên bo mạch (không cần nối ESP32) |
| **14, 21** | `LED_VDD` | Dương nguồn nuôi LED | Nối nguồn **5V công suất ngoài** |
| **28, 47** | `LED_VSS` | Mass nguồn LED | Nối GND chung |

---

## 3. SƠ ĐỒ ĐẤU NỐI CHI TIẾT VỚI ESP32-C3

ESP32-C3 sử dụng các chân GPIO an toàn, không vướng cổng nạp USB CDC/JTAG:

```
    [ESP32-C3]                        [Mạch 4x HT16D35A]
  +------------+                      +------------------+
  |    GND     |--------------------->|  GND (Chân 6)    | <--- Nối chung Mass Nguồn 5V
  |    3.3V    |--------------------->|  VDD (Chân 7)    | (Nguồn logic)
  |   GPIO 3   |--------------------->|  CLK (Chân 10)   | (Clock chung 4 IC)
  |   GPIO 4   |--------------------->|  DIO (Chân 9)    | (Data chung 4 IC)
  |   GPIO 5   |--------------------->|  CS1 (Chân 11 IC1)|
  |   GPIO 6   |--------------------->|  CS2 (Chân 11 IC2)|
  |   GPIO 7   |--------------------->|  CS3 (Chân 11 IC3)|
  |   GPIO 10  |--------------------->|  CS4 (Chân 11 IC4)|
  +------------+                      +------------------+
                                      | LED_VDD (14, 21) | <--- (+) Nguồn 5V Ngoài (2A - 5A)
                                      +------------------+
```

### ⚠️ Quy tắc cấp nguồn an toàn:
* **LED_VDD (Chân 14, 21):** Phải dùng **Nguồn 5V ngoài** riêng (hoặc củ sạc 5V/2A trở lên). Tuyệt đối **KHÔNG** lấy nguồn LED từ chân 3.3V của ESP32 để tránh sụt áp gây reset hoặc cháy IC nguồn ESP32.
* **VDD (Chân 7) và LED_VDD có nối chung được không?**
  * **Được:** Nếu nối chung cả hai vào nguồn 5V ngoài. Lúc này cấp luôn 5V đó vào chân `VIN / 5V` của ESP32.
  * **Tốt nhất:** Tách riêng `VDD` nối 3.3V của ESP32, còn `LED_VDD` nối 5V ngoài để mức tín hiệu logic chuẩn 100% 3.3V.

---

## 4. BẢNG MÃ LỆNH CHUẨN CỦA HT16D35A (Datasheet)

* `0x01`: **Software Reset** - Khởi động lại thanh ghi IC.
* `0x34` + `0x01`: **Master Mode** - IC phát xung đồng bộ SYNC.
* `0x34` + `0x03`: **Slave Mode** - IC nhận xung đồng bộ SYNC từ Master.
* `0x31` + `0x07`: **COM Number** - Bật đầy đủ 8 cổng COM (COM0 - COM7).
* `0x30` + `0x00`: **Binary Mode** - Chế độ hiển thị bật/tắt ma trận cơ bản.
* `0x38` + `0x40`: **Global Brightness** - Đặt độ sáng PWM tối đa (64/64).
* `0x35` + `0x03`: **System Control** - Bật bộ dao động (`OSC ON`) và bật hiển thị (`Display ON`).
* `0x80` + `Address` + `Data...`: **Write DDRAM** - Ghi dữ liệu ma trận LED vào RAM hiển thị.

---

## 5. CÁC CÁCH BIÊN DỊCH VÀ NẠP FIRMWARE

### Cách 1: Nạp tự động qua Cloud GitHub Actions (ESP-IDF v5.1)
Trong thư mục đã có sẵn script nạp tự động:
1. Cắm cáp USB kết nối ESP32-C3 vào máy tính (Cổng `COM6`).
2. Nhấp đúp chuột chạy file: **`build_and_flash.bat`**
3. Hệ thống sẽ tự động đồng bộ code lên GitHub, biên dịch bằng ESP-IDF container và tự động nạp thẳng vào ESP32-C3.

### Cách 2: Nạp bằng phần mềm Arduino IDE
Nếu bạn quen dùng Arduino IDE:
1. Mở file: **`HT16D35A_ESP32C3.ino`**
2. Chọn Board: **ESP32C3 Dev Module**
3. Chọn đúng cổng COM và nhấn **Upload**.
