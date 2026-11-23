# -*- coding: utf-8 -*-
import os
from PIL import Image, ImageDraw, ImageFont

def create_tqfn_pinout_image(output_paths):
    W, H = 2200, 1380
    img = Image.new('RGB', (W, H), color='#070E1F')
    draw = ImageDraw.Draw(img)

    # Load Fonts
    font_title = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 42)
    font_subtitle = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 22)
    font_section = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 24)
    font_chip_brand = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 26)
    font_chip_part = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 34)
    font_chip_sub = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 20)
    font_pad_num = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 16)
    font_pad_name = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 14)
    font_callout_t = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 18)
    font_callout_d = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 15)
    font_step_title = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 20)
    font_step_desc = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 16)

    # 1. Header Banner
    draw.rectangle([(0, 0), (W, 115)], fill='#0F1A36')
    draw.line([(0, 115), (W, 115)], fill='#38BDF8', width=3)
    draw.text((W//2, 40), "SƠ ĐỒ CHÂN IC DÁN VUÔNG PI4IOE5V6416 (TQFN-24 / QFN 4x4mm)", fill='#FFFFFF', font=font_title, anchor='mm')
    draw.text((W//2, 85), "CHÚ Ý: TOÀN BỘ CHÂN GIAO TIẾP VỚI ESP32 (SCL, SDA, NGUỒN 3.3V) NẰM Ở CẠNH TRÊN (CHÂN 19 ĐẾN 24)", fill='#F59E0B', font=font_subtitle, anchor='mm')

    # ==========================================
    # VẼ IC DÁN VUÔNG TQFN-24 (BÊN TRÁI)
    # ==========================================
    cx, cy = 600, 820
    size = 520
    chip_left = cx - size // 2
    chip_top = cy - size // 2
    chip_right = cx + size // 2
    chip_bottom = cy + size // 2

    # Thân IC vuông góc bo nhẹ
    draw.rounded_rectangle([(chip_left, chip_top), (chip_right, chip_bottom)], radius=14, fill='#131A29', outline='#475569', width=4)

    # Vát mép góc Pin 1 & Dấu chấm tròn Pin 1
    draw.polygon([(chip_left, chip_top + 40), (chip_left + 40, chip_top), (chip_left, chip_top)], fill='#070E1F')
    draw.line([(chip_left, chip_top + 40), (chip_left + 40, chip_top)], fill='#F59E0B', width=3)
    draw.ellipse([(chip_left + 35, chip_top + 35), (chip_left + 60, chip_top + 60)], fill='#F59E0B', outline='#B45309', width=2)
    draw.text((chip_left + 70, chip_top + 48), "PIN 1", fill='#F59E0B', font=font_pad_name, anchor='lm')

    # Thermal Pad ở giữa (Đồng mạ thiếc - nối GND)
    tpad_size = 200
    t_left = cx - tpad_size // 2
    t_top = cy - tpad_size // 2
    draw.rounded_rectangle([(t_left, t_top), (t_left + tpad_size, t_top + tpad_size)], radius=10, fill='#1E293B', outline='#3B82F6', width=2)
    draw.text((cx, cy - 35), "THERMAL PAD", fill='#38BDF8', font=font_pad_name, anchor='mm')
    draw.text((cx, cy - 5), "BỤNG IC = MASS GND", fill='#FFFFFF', font=font_callout_t, anchor='mm')
    draw.text((cx, cy + 28), "Đo bíp về chân GND ESP32", fill='#94A3B8', font=font_pad_name, anchor='mm')

    # Chữ in laser trên mặt IC
    draw.text((cx, cy - 160), "DIODES", fill='#64748B', font=font_chip_brand, anchor='mm')
    draw.text((cx, cy - 125), "PI4IOE5V", fill='#E2E8F0', font=font_chip_part, anchor='mm')
    draw.text((cx, cy + 145), "6416ZDEX (TQFN-24)", fill='#38BDF8', font=font_chip_sub, anchor='mm')

    # Kích thước Pad
    pad_len = 36
    pad_thick = 26
    step = 76  # khoảng cách đều

    # 1. CẠNH TRÁI: Chân 1 đến 6 (đi từ TRÊN xuống DƯỚI)
    left_side = [
        (1, "P0_0", "CS1", "Nối CS1 (LED IC 1)"),
        (2, "P0_1", "CS2", "Nối CS2 (LED IC 2)"),
        (3, "P0_2", "CS3", "Nối CS3 (LED IC 3)"),
        (4, "P0_3", "CS4", "Nối CS4 (LED IC 4)"),
        (5, "P0_4", "GPIO", ""),
        (6, "P0_5", "GPIO", ""),
    ]

    for idx, (pnum, pname, role, pdesc) in enumerate(left_side):
        py = int(chip_top + 70 + idx * step)
        draw.rectangle([(chip_left - pad_len, py - pad_thick//2), (chip_left, py + pad_thick//2)], fill='#CBD5E1', outline='#64748B', width=2)
        draw.text((chip_left + 12, py), str(pnum), fill='#F1F5F9', font=font_pad_num, anchor='lm')
        draw.text((chip_left + 36, py), pname, fill='#38BDF8' if "CS" in role else '#94A3B8', font=font_pad_name, anchor='lm')

        # Callout cho 4 chân CS1..CS4 (ở nửa dưới bên trái)
        if "CS" in role:
            bw, bh = 260, 46
            bx = chip_left - pad_len - 35 - bw
            by = py - bh // 2
            draw.rounded_rectangle([(bx, by), (bx + bw, by + bh)], radius=8, fill='#0C2238', outline='#0EA5E9', width=2)
            draw.text((bx + 12, by + 13), f"Chân {pnum}: {pname} -> {role}", fill='#38BDF8', font=font_callout_t, anchor='lm')
            draw.text((bx + 12, by + 32), pdesc, fill='#CBD5E1', font=font_callout_d, anchor='lm')
            draw.line([(bx + bw, py), (chip_left - pad_len, py)], fill='#0EA5E9', width=2)

    # 2. CẠNH ĐÁY: Chân 7 đến 12 (đi từ TRÁI sang PHẢI)
    bottom_side = [
        (7, "P0_6", "GPIO", ""),
        (8, "P0_7", "GPIO", ""),
        (9, "VSS", "GND", "MASS GND VỀ ESP32"),
        (10, "P1_0", "GPIO", ""),
        (11, "P1_1", "GPIO", ""),
        (12, "P1_2", "GPIO", "")
    ]

    for idx, (pnum, pname, role, pdesc) in enumerate(bottom_side):
        px = int(chip_left + 70 + idx * step)
        draw.rectangle([(px - pad_thick//2, chip_bottom), (px + pad_thick//2, chip_bottom + pad_len)], fill='#CBD5E1', outline='#64748B', width=2)
        draw.text((px, chip_bottom - 16), str(pnum), fill='#F1F5F9', font=font_pad_num, anchor='mm')
        draw.text((px, chip_bottom - 36), pname, fill='#3B82F6' if role == 'GND' else '#94A3B8', font=font_pad_name, anchor='mm')

        if role == 'GND':
            bw, bh = 290, 50
            bx = px - bw // 2
            by = chip_bottom + pad_len + 35
            draw.rounded_rectangle([(bx, by), (bx + bw, by + bh)], radius=8, fill='#0B1E3B', outline='#3B82F6', width=2)
            draw.text((bx + bw//2, by + 15), "Chân 9: VSS (Mass GND)", fill='#60A5FA', font=font_callout_t, anchor='mm')
            draw.text((bx + bw//2, by + 35), "ĐO BÍP VỀ CHÂN GND ESP32", fill='#FFFFFF', font=font_callout_d, anchor='mm')
            draw.line([(px, chip_bottom + pad_len), (px, by)], fill='#3B82F6', width=3)

    # 3. CẠNH PHẢI: Chân 13 đến 18 (đi từ DƯỚI lên TRÊN)
    right_side = [
        (18, "ADDR", "ADDR"),
        (17, "P1_7", "GPIO"),
        (16, "P1_6", "GPIO"),
        (15, "P1_5", "GPIO"),
        (14, "P1_4", "GPIO"),
        (13, "P1_3", "GPIO")
    ]

    for idx, (pnum, pname, role) in enumerate(right_side):
        py = int(chip_bottom - 70 - idx * step)
        draw.rectangle([(chip_right, py - pad_thick//2), (chip_right + pad_len, py + pad_thick//2)], fill='#CBD5E1', outline='#64748B', width=2)
        draw.text((chip_right - 12, py), str(pnum), fill='#F1F5F9', font=font_pad_num, anchor='rm')
        draw.text((chip_right - 36, py), pname, fill='#10B981' if role == 'ADDR' else '#94A3B8', font=font_pad_name, anchor='rm')

    # 4. CẠNH TRÊN: Chân 19 đến 24 (đi từ PHẢI sang TRÁI)
    top_side = [
        (19, "SCL", "#F59E0B"),
        (20, "SDA", "#F59E0B"),
        (21, "VDD_P", "#F97316"),
        (22, "INT#", "#06B6D4"),
        (23, "VDD_I2C", "#EF4444"),
        (24, "RESET#", "#A855F7")
    ]

    for idx, (pnum, pname, col) in enumerate(top_side):
        px = int(chip_right - 70 - idx * step)
        draw.rectangle([(px - pad_thick//2, chip_top - pad_len), (px + pad_thick//2, chip_top)], fill='#CBD5E1', outline='#64748B', width=2)
        draw.text((px, chip_top + 16), str(pnum), fill='#F1F5F9', font=font_pad_num, anchor='mm')
        draw.text((px, chip_top + 36), pname, fill=col, font=font_pad_name, anchor='mm')

    # ==============================================================
    # BỐ TRÍ CÁC HỘP CALLOUT CẠNH TRÊN & 2 BÊN GỌN GÀNG, KHÔNG CHÉO NHAU
    # ==============================================================
    
    # Tọa độ các chân trên cạnh trên:
    p19_x = int(chip_right - 70)
    p20_x = int(chip_right - 70 - step)
    p21_x = int(chip_right - 70 - 2 * step)
    p22_x = int(chip_right - 70 - 3 * step)
    p23_x = int(chip_right - 70 - 4 * step)
    p24_x = int(chip_right - 70 - 5 * step)
    p18_y = int(chip_bottom - 70 - 5 * step) # = chip_top + 70

    # 1. CỤM BÊN PHẢI (I2C): Chân 19 (SCL), Chân 20 (SDA), Chân 18 (ADDR)
    # Box Chân 19: SCL
    b19_x, b19_y, b19_w, b19_h = chip_right + 55, chip_top - 120, 310, 52
    draw.rounded_rectangle([(b19_x, b19_y), (b19_x + b19_w, b19_y + b19_h)], radius=8, fill='#2B1C06', outline='#F59E0B', width=2)
    draw.text((b19_x + 12, b19_y + 14), "Chân 19: SCL (Clock I2C)", fill='#FBBF24', font=font_callout_t, anchor='lm')
    draw.text((b19_x + 12, b19_y + 36), "ĐO BÍP VỀ SCL ESP32 (GPIO 5)", fill='#FEF08A', font=font_callout_d, anchor='lm')
    draw.line([(b19_x, b19_y + b19_h//2), (p19_x, chip_top - pad_len)], fill='#F59E0B', width=2)

    # Box Chân 20: SDA
    b20_x, b20_y, b20_w, b20_h = chip_right + 55, chip_top - 40, 310, 52
    draw.rounded_rectangle([(b20_x, b20_y), (b20_x + b20_w, b20_y + b20_h)], radius=8, fill='#2B1C06', outline='#F59E0B', width=2)
    draw.text((b20_x + 12, b20_y + 14), "Chân 20: SDA (Data I2C)", fill='#FBBF24', font=font_callout_t, anchor='lm')
    draw.text((b20_x + 12, b20_y + 36), "ĐO BÍP VỀ SDA ESP32 (GPIO 4)", fill='#FEF08A', font=font_callout_d, anchor='lm')
    draw.line([(b20_x, b20_y + b20_h//2), (p20_x, chip_top - pad_len)], fill='#F59E0B', width=2)

    # Box Chân 18: ADDR
    b18_x, b18_y, b18_w, b18_h = chip_right + 55, chip_top + 45, 310, 50
    draw.rounded_rectangle([(b18_x, b18_y), (b18_x + b18_w, b18_y + b18_h)], radius=8, fill='#062319', outline='#10B981', width=2)
    draw.text((b18_x + 12, b18_y + 14), "Chân 18: ADDR (Địa chỉ I2C)", fill='#34D399', font=font_callout_t, anchor='lm')
    draw.text((b18_x + 12, b18_y + 34), "Nối GND (0x20) hoặc 3.3V (0x21)", fill='#E2E8F0', font=font_callout_d, anchor='lm')
    draw.line([(b18_x, b18_y + b18_h//2), (chip_right + pad_len, p18_y)], fill='#10B981', width=2)

    # 2. CỤM BÊN TRÁI TRÊN: Chân 24 (RESET) & Chân 23 (VDD_I2C 3.3V)
    # Box Chân 24: RESET
    b24_x, b24_y, b24_w, b24_h = chip_left - 330, chip_top - 120, 290, 50
    draw.rounded_rectangle([(b24_x, b24_y), (b24_x + b24_w, b24_y + b24_h)], radius=8, fill='#1B0D2A', outline='#A855F7', width=2)
    draw.text((b24_x + 12, b24_y + 14), "Chân 24: RESET# (Reset)", fill='#C084FC', font=font_callout_t, anchor='lm')
    draw.text((b24_x + 12, b24_y + 34), "Kéo lên 3.3V hoặc nối GPIO Reset", fill='#E2E8F0', font=font_callout_d, anchor='lm')
    draw.line([(b24_x + b24_w, b24_y + b24_h//2), (p24_x, chip_top - pad_len)], fill='#A855F7', width=2)

    # Box Chân 23: VDD_I2C (Nguồn 3.3V)
    b23_x, b23_y, b23_w, b23_h = chip_left - 330, chip_top - 40, 290, 52
    draw.rounded_rectangle([(b23_x, b23_y), (b23_x + b23_w, b23_y + b23_h)], radius=8, fill='#2B0D0D', outline='#EF4444', width=2)
    draw.text((b23_x + 12, b23_y + 14), "Chân 23: VDD_I2C (Nguồn)", fill='#F87171', font=font_callout_t, anchor='lm')
    draw.text((b23_x + 12, b23_y + 36), "ĐO BÍP VỀ CHÂN 3.3V ESP32!", fill='#FFFFFF', font=font_callout_d, anchor='lm')
    draw.line([(b23_x + b23_w, b23_y + b23_h//2), (p23_x, chip_top - pad_len)], fill='#EF4444', width=2)

    # 3. CỤM GIỮA TRÊN: Chân 22 (INT#) & Chân 21 (VDD_P)
    # Box Chân 22 (INT#)
    b22_x, b22_y, b22_w, b22_h = cx - 180, chip_top - 200, 260, 50
    draw.rounded_rectangle([(b22_x, b22_y), (b22_x + b22_w, b22_y + b22_h)], radius=8, fill='#081E28', outline='#06B6D4', width=2)
    draw.text((b22_x + 12, b22_y + 14), "Chân 22: INT# (Báo ngắt)", fill='#22D3EE', font=font_callout_t, anchor='lm')
    draw.text((b22_x + 12, b22_y + 34), "Nối GPIO ngắt (Tùy chọn)", fill='#E2E8F0', font=font_callout_d, anchor='lm')
    draw.line([(b22_x + b22_w//2, b22_y + b22_h), (p22_x, chip_top - pad_len)], fill='#06B6D4', width=2)

    # Box Chân 21 (VDD_P)
    b21_x, b21_y, b21_w, b21_h = cx + 110, chip_top - 200, 260, 50
    draw.rounded_rectangle([(b21_x, b21_y), (b21_x + b21_w, b21_y + b21_h)], radius=8, fill='#281308', outline='#F97316', width=2)
    draw.text((b21_x + 12, b21_y + 14), "Chân 21: VDD(P) (Nguồn LED)", fill='#FB923C', font=font_callout_t, anchor='lm')
    draw.text((b21_x + 12, b21_y + 34), "Cấp 3.3V hoặc 5V ngoài cho Port", fill='#E2E8F0', font=font_callout_d, anchor='lm')
    draw.line([(b21_x + b21_w//2, b21_y + b21_h), (p21_x, chip_top - pad_len)], fill='#F97316', width=2)

    # ==========================================
    # CỘT PHẢI: BẢNG HƯỚNG DẪN ĐO & CHECKLIST
    # ==========================================
    gx = 1270
    gw = 880

    # Khung Hướng dẫn đo
    draw.rounded_rectangle([(gx, 150), (gx + gw, 720)], radius=14, fill='#101935', outline='#38BDF8', width=2)
    draw.rectangle([(gx, 150), (gx + gw, 210)], fill='#1E293B')
    draw.text((gx + gw//2, 180), "QUY TRÌNH DÙNG ĐỒNG HỒ VOM ĐO BÍP IC VUÔNG TQFN-24", fill='#38BDF8', font=font_section, anchor='mm')

    steps = [
        ("BƯỚC 1: XÁC ĐỊNH DẤU CHẤM PIN 1", "Nhìn mặt trên IC vuông, tìm DẤU CHẤM TRÒN LÕM hoặc góc vát mép.\nĐó là PIN 1 (Góc trên cùng bên trái). Đếm ngược chiều kim đồng hồ từ 1 đến 24.", '#F59E0B'),
        ("BƯỚC 2: ĐO CẶP CHÂN I2C Ở CẠNH TRÊN (QUAN TRỌNG NHẤT)", "• Chân 19 (SCL): Nằm sát góc trên bên phải -> Rà que kia qua ESP32 (nghe BÍP).\n• Chân 20 (SDA): Nằm ngay cạnh Chân 19 -> Rà que kia qua ESP32 (nghe BÍP).", '#F59E0B'),
        ("BƯỚC 3: ĐO NGUỒN 3.3V LOGIC (CHÂN 23)", "Chân 23 (sát góc trên bên trái, cạnh chân 24) BẮT BUỘC phải thông mạch với 3.3V ESP32.", '#EF4444'),
        ("BƯỚC 4: ĐO MASS GND (CHÂN 9 HOẶC BỤNG IC)", "Đo chân 9 (ở giữa cạnh đáy) hoặc chấm que vào bụng thiếc IC -> Phải thông mạch về GND ESP32.", '#3B82F6'),
        ("BƯỚC 5: KIỂM TRA 4 CHÂN CS CẠNH TRÁI (CHÂN 1 -> 4)", "Chân 1 (CS1), Chân 2 (CS2), Chân 3 (CS3), Chân 4 (CS4) sẽ dẫn thẳng tới chân CSB (Chân 11) của 4 chip LED HT16D35A!", '#38BDF8')
    ]

    sy = 230
    for title, desc, col in steps:
        draw.text((gx + 30, sy), title, fill=col, font=font_step_title)
        lines = desc.split('\n')
        for l_idx, line in enumerate(lines):
            draw.text((gx + 30, sy + 28 + l_idx * 22), line, fill='#CBD5E1', font=font_step_desc)
        sy += 95

    # Khung Lưu ý an toàn & địa chỉ I2C
    draw.rounded_rectangle([(gx, 750), (gx + gw, 1280)], radius=14, fill='#111827', outline='#475569', width=2)
    draw.rectangle([(gx, 750), (gx + gw, 810)], fill='#1F2937')
    draw.text((gx + gw//2, 780), "BẢNG ĐỊA CHỈ I2C & MẸO ĐO CHÂN SIÊU NHỎ", fill='#F3F4F6', font=font_section, anchor='mm')

    notes = [
        ("Địa chỉ I2C của chip vuông", "• Địa chỉ 7-bit mặc định là 0x20 nếu Chân 18 (ADDR) nối mass GND.\n• Địa chỉ 7-bit đổi thành 0x21 nếu Chân 18 (ADDR) nối lên nguồn 3.3V.", '#10B981'),
        ("Chân 24 (RESET#)", "Phải được kéo lên 3.3V (hoặc nối GPIO ESP32 ở mức HIGH). Nếu chân này bị hở hoặc chạm 0V, chip sẽ bị treo/không nhận lệnh I2C!", '#A855F7'),
        ("Chân 21 (VDD_P)", "Nguồn cấp cho 16 chân GPIO. Nếu điều khiển LED/tải 5V thì chân này nối 5V ngoài. ESP32 vẫn an toàn vì chân 23 đã nối riêng 3.3V.", '#F97316'),
        ("Mẹo đo chân gầm phẳng TQFN cực hay", "Vì IC vuông 4x4mm có chân phẳng dưới gầm rất nhỏ, đầu que đo VOM thông thường dễ bị trượt hoặc quẹt chập 2 chân.\n-> Mẹo: Lấy 1 chiếc KIM KHÂU kẹp chặt vào đầu que đo, dùng mũi kim chấm thẳng vào pad đồng của chân trên bo mạch!", '#38BDF8')
    ]

    ny = 830
    for title, desc, col in notes:
        draw.text((gx + 30, ny), title, fill=col, font=font_step_title)
        lines = desc.split('\n')
        for l_idx, line in enumerate(lines):
            draw.text((gx + 30, ny + 28 + l_idx * 22), line, fill='#CBD5E1', font=font_step_desc)
        ny += 105

    for p in output_paths:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        img.save(p, quality=95)
        print(f"Generated successfully: {p}")

if __name__ == '__main__':
    paths = [
        r"d:\thien\01_DuAn_Code\espc3 - led\PI4IOE5V6416_pinout.png",
        r"C:\Users\HG\.gemini\antigravity\brain\63bb32c4-ab52-43a7-9e1e-e5c849be6187\PI4IOE5V6416_pinout.png"
    ]
    create_tqfn_pinout_image(paths)
