# -*- coding: utf-8 -*-
import os
from PIL import Image, ImageDraw, ImageFont

def create_pinout_image(output_paths):
    # Kích thước ảnh siêu nét: 2000 x 1200
    W, H = 2000, 1200
    img = Image.new('RGB', (W, H), color='#0A0F1D')
    draw = ImageDraw.Draw(img)

    # Load Fonts Windows
    font_title = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 40)
    font_subtitle = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 22)
    font_section = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 24)
    font_chip_brand = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 26)
    font_chip_part = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 32)
    font_chip_sub = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 20)
    font_pin_num = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 18)
    font_pin_name = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 16)
    font_callout_t = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 18)
    font_callout_d = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 15)
    font_step_title = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 20)
    font_step_desc = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 16)
    font_tbl_h = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 18)
    font_tbl_b = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 17)

    # 1. Header Banner
    draw.rectangle([(0, 0), (W, 110)], fill='#131D38')
    draw.line([(0, 110), (W, 110)], fill='#2563EB', width=2)
    draw.text((W//2, 38), "SƠ ĐỒ CHÂN IC PI4IOE5V6416 & CÁC ĐIỂM ĐO VỀ ESP32-C3", fill='#FFFFFF', font=font_title, anchor='mm')
    draw.text((W//2, 82), "ĐẶC BIỆT CHÚ Ý: Đo thông mạch cặp chân I2C (SCL - Chân 22 & SDA - Chân 23) và Nguồn 3.3V (Chân 2)", fill='#38BDF8', font=font_subtitle, anchor='mm')

    # ==========================
    # PHẦN VẼ CHIP TSSOP-24 (BÊN TRÁI)
    # ==========================
    cx, cy = 600, 640 # Tâm thân IC
    chip_w, chip_h = 300, 780
    chip_left = cx - chip_w // 2
    chip_top = cy - chip_h // 2
    chip_right = cx + chip_w // 2
    chip_bottom = cy + chip_h // 2

    # Thân IC
    draw.rounded_rectangle([(chip_left, chip_top), (chip_right, chip_bottom)], radius=18, fill='#171C26', outline='#475569', width=4)

    # Dấu khuyết đỉnh IC (Orientation Notch)
    notch_r = 28
    draw.chord([(cx - notch_r, chip_top - 14), (cx + notch_r, chip_top + 42)], start=0, end=180, fill='#0A0F1D', outline='#475569', width=3)

    # Dấu chấm Pin 1 (Index Dot)
    draw.ellipse([(chip_left + 22, chip_top + 34), (chip_left + 46, chip_top + 58)], fill='#F59E0B', outline='#B45309', width=2)

    # Chữ in laser trên mặt IC
    draw.text((cx, cy - 90), "DIODES", fill='#64748B', font=font_chip_brand, anchor='mm')
    draw.text((cx, cy - 45), "PI4IOE5V", fill='#E2E8F0', font=font_chip_part, anchor='mm')
    draw.text((cx, cy + 5), "6416LEX", fill='#38BDF8', font=font_chip_part, anchor='mm')
    draw.text((cx, cy + 55), "TSSOP-24", fill='#94A3B8', font=font_chip_sub, anchor='mm')
    draw.text((cx, cy + 95), "(Vỏ dẹp chân rết)", fill='#64748B', font=font_callout_d, anchor='mm')

    # Danh sách 24 chân
    left_pins = [
        (1, "INT#", "OPT_INT", "Chân 1: INT# (Ngắt)", "Báo ngắt về GPIO ESP32"),
        (2, "VDD_I2C", "PWR_3V3", "Chân 2: VDD_I2C (Nguồn 3.3V)", "ĐO BÍP VỀ CHÂN 3.3V ESP32"),
        (3, "RESET#", "RESET", "Chân 3: RESET# (Reset)", "Kéo lên 3.3V hoặc nối GPIO"),
        (4, "P0_0", "GPIO", "P0_0", "CS1 / Tải"),
        (5, "P0_1", "GPIO", "P0_1", "CS2 / Tải"),
        (6, "P0_2", "GPIO", "P0_2", "CS3 / Tải"),
        (7, "P0_3", "GPIO", "P0_3", "CS4 / Tải"),
        (8, "P0_4", "GPIO", "P0_4", "GPIO"),
        (9, "P0_5", "GPIO", "P0_5", "GPIO"),
        (10, "P0_6", "GPIO", "P0_6", "GPIO"),
        (11, "P0_7", "GPIO", "P0_7", "GPIO"),
        (12, "VSS", "GND", "Chân 12: VSS (Mass 0V)", "ĐO BÍP VỀ CHÂN GND ESP32")
    ]

    right_pins = [
        (24, "VDD_P", "PWR_EXT", "Chân 24: VDD_P (Nguồn Port)", "Nối nguồn 3.3V hoặc 5V ngoài"),
        (23, "SDA", "I2C_SDA", "Chân 23: SDA (Data I2C)", "ĐO BÍP VỀ GPIO SDA ESP32"),
        (22, "SCL", "I2C_SCL", "Chân 22: SCL (Clock I2C)", "ĐO BÍP VỀ GPIO SCL ESP32"),
        (21, "ADDR", "ADDR", "Chân 21: ADDR (Địa chỉ)", "Nối GND (0x20) hoặc 3.3V (0x21)"),
        (20, "P1_7", "GPIO", "P1_7", "GPIO"),
        (19, "P1_6", "GPIO", "P1_6", "GPIO"),
        (18, "P1_5", "GPIO", "P1_5", "GPIO"),
        (17, "P1_4", "GPIO", "P1_4", "GPIO"),
        (16, "P1_3", "GPIO", "P1_3", "GPIO"),
        (15, "P1_2", "GPIO", "P1_2", "GPIO"),
        (14, "P1_1", "GPIO", "P1_1", "GPIO"),
        (13, "P1_0", "GPIO", "P1_0", "GPIO")
    ]

    pin_step = (chip_h - 120) / 11
    start_y = chip_top + 60

    def get_color_scheme(ptype):
        if ptype in ("I2C_SCL", "I2C_SDA"):
            return "#D97706", "#F59E0B", "#251806", "#FFFFFF" # Cam vàng
        elif ptype == "PWR_3V3":
            return "#DC2626", "#EF4444", "#260808", "#FFFFFF" # Đỏ nguồn
        elif ptype == "GND":
            return "#2563EB", "#3B82F6", "#0B1528", "#FFFFFF" # Xanh dương Mass
        elif ptype == "RESET":
            return "#9333EA", "#A855F7", "#1E0A2D", "#FFFFFF" # Tím Reset
        elif ptype == "OPT_INT":
            return "#0891B2", "#06B6D4", "#081E24", "#FFFFFF" # Cyan Ngắt
        elif ptype == "ADDR":
            return "#059669", "#10B981", "#082017", "#FFFFFF" # Xanh lá
        elif ptype == "PWR_EXT":
            return "#EA580C", "#F97316", "#261108", "#FFFFFF" # Cam đỏ
        else:
            return "#475569", "#64748B", "#1E222A", "#94A3B8" # Xám

    # VẼ CÁC CHÂN BÊN TRÁI (1 -> 12)
    for i, (pnum, pname, ptype, ptitle, pdesc) in enumerate(left_pins):
        py = int(start_y + i * pin_step)
        c_border, c_accent, c_bg, c_txt = get_color_scheme(ptype)
        is_hl = ptype in ("PWR_3V3", "GND", "RESET", "OPT_INT")

        # Chân chìa ra
        draw.rectangle([(chip_left - 40, py - 10), (chip_left, py + 10)], fill='#CBD5E1', outline='#64748B', width=2)
        
        # Nhãn số và tên chân trong IC
        draw.text((chip_left + 15, py), str(pnum), fill='#CBD5E1', font=font_pin_num, anchor='lm')
        draw.text((chip_left + 45, py), pname, fill=c_accent if is_hl else '#94A3B8', font=font_pin_name, anchor='lm')

        if is_hl:
            # Hộp callout 2 dòng gọn gàng
            bw, bh = 340, 52
            bx = chip_left - 60 - bw
            by = py - bh // 2
            draw.rounded_rectangle([(bx, by), (bx + bw, by + bh)], radius=8, fill=c_bg, outline=c_accent, width=2)
            draw.text((bx + 12, by + 14), ptitle, fill=c_accent, font=font_callout_t, anchor='lm')
            draw.text((bx + 12, by + 36), pdesc, fill='#F8FAFC', font=font_callout_d, anchor='lm')
            # Đường nối mũi tên
            draw.line([(bx + bw, py), (chip_left - 40, py)], fill=c_accent, width=3)
        else:
            draw.text((chip_left - 50, py), pname, fill='#475569', font=font_pin_name, anchor='rm')

    # VẼ CÁC CHÂN BÊN PHẢI (24 -> 13)
    for i, (pnum, pname, ptype, ptitle, pdesc) in enumerate(right_pins):
        py = int(start_y + i * pin_step)
        c_border, c_accent, c_bg, c_txt = get_color_scheme(ptype)
        is_hl = ptype in ("I2C_SCL", "I2C_SDA", "ADDR", "PWR_EXT")

        # Chân chìa ra
        draw.rectangle([(chip_right, py - 10), (chip_right + 40, py + 10)], fill='#CBD5E1', outline='#64748B', width=2)

        # Nhãn số và tên chân trong IC
        draw.text((chip_right - 15, py), str(pnum), fill='#CBD5E1', font=font_pin_num, anchor='rm')
        draw.text((chip_right - 45, py), pname, fill=c_accent if is_hl else '#94A3B8', font=font_pin_name, anchor='rm')

        if is_hl:
            # Hộp callout 2 dòng gọn gàng
            bw, bh = 340, 52
            bx = chip_right + 60
            by = py - bh // 2
            draw.rounded_rectangle([(bx, by), (bx + bw, by + bh)], radius=8, fill=c_bg, outline=c_accent, width=2)
            draw.text((bx + 12, by + 14), ptitle, fill=c_accent, font=font_callout_t, anchor='lm')
            draw.text((bx + 12, by + 36), pdesc, fill='#FEF08A' if 'I2C' in ptype else '#F8FAFC', font=font_callout_d, anchor='lm')
            # Đường nối mũi tên
            draw.line([(chip_right + 40, py), (bx, py)], fill=c_accent, width=3)
        else:
            draw.text((chip_right + 50, py), pname, fill='#475569', font=font_pin_name, anchor='lm')

    # ==========================
    # CỘT PHẢI: BẢNG HƯỚNG DẪN ĐO & TRA CỨU
    # ==========================
    gx = 1200
    gw = 750

    # Khung Hướng dẫn đo
    draw.rounded_rectangle([(gx, 140), (gx + gw, 600)], radius=14, fill='#111A30', outline='#38BDF8', width=2)
    draw.rectangle([(gx, 140), (gx + gw, 195)], fill='#1E293B')
    draw.text((gx + gw//2, 168), "HƯỚNG DẪN DÙNG ĐỒNG HỒ VOM ĐO THÔNG MẠCH", fill='#38BDF8', font=font_section, anchor='mm')

    steps = [
        ("BƯỚC 1: TẮT NGUỒN HOÀN TOÀN", "Rút cáp USB & ngắt nguồn bo mạch trước khi đo thông mạch (nấc BÍP / DIODE).", '#EF4444'),
        ("BƯỚC 2: ĐO TÌM CHÂN CLOCK (SCL)", "Đặt 1 que que vào Chân 22 (SCL). Cầm que kia rà lần lượt các GPIO của ESP32.\n-> Chân GPIO nào đồng hồ KÊU BÍP chính là chân SCL đã nối!", '#F59E0B'),
        ("BƯỚC 3: ĐO TÌM CHÂN DATA (SDA)", "Đặt 1 que vào Chân 23 (SDA). Cầm que kia rà lần lượt các GPIO của ESP32.\n-> Chân GPIO nào đồng hồ KÊU BÍP chính là chân SDA đã nối!", '#F59E0B'),
        ("BƯỚC 4: KIỂM TRA NGUỒN 3.3V LOGIC", "Chân 2 (VDD_I2C) BẮT BUỘC phải thông mạch (BÍP) với chân 3.3V của ESP32.", '#38BDF8'),
        ("BƯỚC 5: KIỂM TRA MASS CHUNG (GND)", "Chân 12 (VSS) BẮT BUỘC phải thông mạch (BÍP) với chân GND của ESP32.", '#38BDF8')
    ]

    sy = 215
    for title, desc, col in steps:
        draw.text((gx + 25, sy), title, fill=col, font=font_step_title)
        lines = desc.split('\n')
        for l_idx, line in enumerate(lines):
            draw.text((gx + 25, sy + 26 + l_idx * 20), line, fill='#CBD5E1', font=font_step_desc)
        sy += 75

    # Khung Bảng Tra Cứu TQFN-24 (IC Vuông)
    draw.rounded_rectangle([(gx, 625), (gx + gw, 1140)], radius=14, fill='#111827', outline='#475569', width=2)
    draw.rectangle([(gx, 625), (gx + gw, 680)], fill='#1F2937')
    draw.text((gx + gw//2, 652), "BẢNG ĐỐI CHIẾU NẾU MẠCH DÙNG IC VUÔNG (TQFN-24)", fill='#F3F4F6', font=font_section, anchor='mm')

    # Bảng cột
    col_x1 = gx + 25
    col_x2 = gx + 260
    col_x3 = gx + 460

    # Header bảng
    draw.text((col_x1, 700), "Tín hiệu cần đo", fill='#94A3B8', font=font_tbl_h)
    draw.text((col_x2, 700), "Chân TSSOP-24", fill='#94A3B8', font=font_tbl_h)
    draw.text((col_x3, 700), "Chân TQFN-24", fill='#94A3B8', font=font_tbl_h)
    draw.line([(gx + 15, 730), (gx + gw - 15, 730)], fill='#374151', width=1)

    tqfn_rows = [
        ("SCL (Clock I2C)", "Chân 22", "Chân 19", '#F59E0B'),
        ("SDA (Data I2C)", "Chân 23", "Chân 20", '#F59E0B'),
        ("VDD_I2C (Nguồn 3.3V)", "Chân 2", "Chân 23", '#EF4444'),
        ("VSS (Mass GND)", "Chân 12", "Chân 9 (và Thermal Pad)", '#3B82F6'),
        ("RESET# (Reset cứng)", "Chân 3", "Chân 24", '#A855F7'),
        ("INT# (Ngắt báo ESP32)", "Chân 1", "Chân 22", '#06B6D4'),
        ("ADDR (Địa chỉ I2C)", "Chân 21", "Chân 18", '#10B981'),
        ("VDD_P (Nguồn LED/Tải)", "Chân 24", "Chân 21", '#F97316')
    ]

    ry = 745
    for sig, tssop_p, tqfn_p, col in tqfn_rows:
        draw.text((col_x1, ry), sig, fill=col, font=font_tbl_b)
        draw.text((col_x2, ry), tssop_p, fill='#FFFFFF', font=font_tbl_b)
        draw.text((col_x3, ry), tqfn_p, fill=col if 'SCL' in sig or 'SDA' in sig else '#FFFFFF', font=font_tbl_b)
        draw.line([(gx + 15, ry + 36), (gx + gw - 15, ry + 36)], fill='#1F2937', width=1)
        ry += 48

    for p in output_paths:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        img.save(p, quality=95)
        print(f"Generated successfully: {p}")

if __name__ == '__main__':
    paths = [
        r"d:\thien\01_DuAn_Code\espc3 - led\PI4IOE5V6416_pinout.png",
        r"C:\Users\HG\.gemini\antigravity\brain\63bb32c4-ab52-43a7-9e1e-e5c849be6187\PI4IOE5V6416_pinout.png"
    ]
    create_pinout_image(paths)
