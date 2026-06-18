#!/usr/bin/env python3
import sys
import os
import time
import serial
import argparse
import subprocess

def hardware_reset():
    print("[INFO] Đang tự động Reset cứng STM32 qua ST-LINK (OpenOCD)...")
    try:
        subprocess.run(
            ["openocd", "-f", "interface/stlink.cfg", "-f", "target/stm32f4x.cfg", "-c", "init", "-c", "reset halt", "-c", "resume", "-c", "exit"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=5
        )
        print("[INFO] Đã gửi lệnh Reset thành công!")
    except Exception as e:
        print(f"[WARN] Không thể reset qua OpenOCD: {e}. Vui lòng nhấn nút RESET vật lý trên board.")



# Sử dụng thuật toán CRC32 phần cứng tương thích STM32 (MPEG-2)
def crc32_stm32(data):
    crc = 0xFFFFFFFF
    poly = 0x04C11DB7
    
    for i in range(0, len(data), 4):
        word_bytes = data[i:i+4]
        if len(word_bytes) < 4:
            word_bytes = word_bytes + b'\xFF' * (4 - len(word_bytes))
        
        word = int.from_bytes(word_bytes, byteorder='little')
        crc ^= word
        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ poly) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc

def send_fota_packet(ser, cmd, payload):
    # Khung truyền: [START_BYTE (0xAA)] [CMD (1 byte)] [LEN (2 bytes)] [PAYLOAD] [CRC32 (4 bytes)]
    payload_len = len(payload)
    header = bytes([0xAA, cmd, (payload_len >> 8) & 0xFF, payload_len & 0xFF])
    
    # Tính toán CRC32 của [CMD] [LEN] [PAYLOAD]
    crc_data = bytes([cmd, (payload_len >> 8) & 0xFF, payload_len & 0xFF]) + payload
    crc_val = crc32_stm32(crc_data)
    
    packet = header + payload + crc_val.to_bytes(4, byteorder='big')
    ser.write(packet)
    return packet

def wait_for_response(ser, timeout_sec=2.0):
    start_time = time.time()
    buff = b""
    while (time.time() - start_time) < timeout_sec:
        if ser.in_waiting > 0:
            byte = ser.read(1)
            # In ký tự debug từ STM32 nếu là ký tự ASCII in được
            if len(byte) > 0:
                b_val = byte[0]
                if 32 <= b_val <= 126 or b_val in [10, 13]:
                    sys.stdout.write(chr(b_val))
                    sys.stdout.flush()
            buff += byte
            
            # Quét tìm byte 0x55 trong bộ đệm
            while len(buff) >= 6:
                idx = buff.find(0x55)
                if idx == -1:
                    buff = buff[-1:]
                    break
                elif idx > 0:
                    buff = buff[idx:]
                    continue
                else:
                    status = buff[1]
                    status_word = bytes([status, 0, 0, 0])
                    expected_crc = crc32_stm32(status_word)
                    received_crc = int.from_bytes(buff[2:6], byteorder='big')
                    
                    if received_crc == expected_crc:
                        return status
                    else:
                        # Sai CRC, đây là ký tự ngẫu nhiên trùng mã, bỏ qua byte 0x55 này và tiếp tục quét
                        buff = buff[1:]
        time.sleep(0.001)
    return None # Timeout

def show_progress(current, total, bar_length=40):
    percent = float(current) / total
    arrow = '=' * int(round(percent * bar_length) - 1) + '>'
    spaces = ' ' * (bar_length - len(arrow))
    sys.stdout.write(f"\rTiến độ: [{arrow + spaces}] {percent*100:.1f}% ({current}/{total} bytes)")
    sys.stdout.flush()

def main():
    parser = argparse.ArgumentParser(description="Công cụ nạp Firmware FOTA qua UART cho STM32F411.")
    parser.add_argument("file", help="Đường dẫn tới file ứng dụng .bin (VD: DUMMY.bin)")
    parser.add_argument("-p", "--port", default="/dev/ttyUSB0", help="Cổng Serial kết nối (mặc định: /dev/ttyUSB0)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Tốc độ Baudrate (mặc định: 115200)")
    parser.add_argument("-c", "--chunk", type=int, default=256, help="Kích thước chunk truyền dữ liệu (mặc định: 256 bytes)")
    args = parser.parse_args()

    if not os.path.exists(args.file):
        print(f"[ERROR] File không tồn tại: {args.file}")
        sys.exit(1)

    # Đọc dữ liệu file nhị phân
    with open(args.file, "rb") as f:
        firmware_data = f.read()

    # Kích thước và CRC32 thực tế
    fw_size = len(firmware_data)
    fw_crc = crc32_stm32(firmware_data)
    
    print("=" * 50)
    print("         FOTA HOST DOWNLOADER TOOL")
    print("=" * 50)
    print(f"File nạp: {args.file}")
    print(f"Kích thước: {fw_size} bytes")
    print(f"Mã CRC32:  0x{fw_crc:08X}")
    print(f"Cổng Serial: {args.port} | Baud: {args.baud} | Chunk Size: {args.chunk} bytes")
    print("=" * 50)

    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except Exception as e:
        print(f"[ERROR] Không thể mở cổng serial: {e}")
        sys.exit(1)

    # Xóa sạch bộ đệm nhận của OS để loại bỏ dữ liệu cũ trước khi Reset
    ser.reset_input_buffer()

    # Tự động gửi lệnh Reset cứng qua OpenOCD
    hardware_reset()

    print("[INFO] Đang đợi tín hiệu đếm ngược từ STM32 Bootloader...")
    print("Mẹo: Nhấn nút RESET trên board để bắt đầu ngay nếu cần.")

    # Chờ bản tin đếm ngược FOTA
    buff = ""
    in_countdown = False
    
    # Thiết lập thời gian chờ tối đa 15s cho việc Reset mạch
    reset_wait_start = time.time()
    while (time.time() - reset_wait_start) < 15.0:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            sys.stdout.write(data)
            sys.stdout.flush()
            buff += data
            if "Starting 10s FOTA countdown" in buff:
                print("\n[INFO] Đã phát hiện tín hiệu đếm ngược Bootloader!")
                in_countdown = True
                break
        time.sleep(0.01)

    # 1. Gửi lệnh FOTA_CMD_START liên tục cho đến khi kết nối thành công (flooding)
    # Cấu trúc payload: [Size (4 bytes)] [CRC32 (4 bytes)] [Version (4 bytes)]
    size_bytes = fw_size.to_bytes(4, byteorder='little')
    crc_bytes = fw_crc.to_bytes(4, byteorder='little')
    version_bytes = (1).to_bytes(4, byteorder='little')
    start_payload = size_bytes + crc_bytes + version_bytes
    
    print("[FOTA] Đang kết nối với Bootloader...")
    ser.reset_input_buffer()
    
    connected = False
    resp_status = None
    start_time = time.time()
    
    # Gửi gói FOTA START mỗi 200ms cho đến khi nhận được chuỗi in ra hoặc gói ACK
    serial_buf = b""
    while (time.time() - start_time) < 20.0:
        send_fota_packet(ser, 0x01, start_payload)
        ser.flush()
        
        # Chờ đọc phản hồi ngắn (150ms)
        read_start = time.time()
        while (time.time() - read_start) < 0.15:
            if ser.in_waiting > 0:
                byte = ser.read(1)
                if len(byte) > 0:
                    serial_buf += byte
                    b_val = byte[0]
                    # In ký tự debug ra màn hình
                    if 32 <= b_val <= 126 or b_val in [10, 13]:
                        sys.stdout.write(chr(b_val))
                        sys.stdout.flush()
                        
            # Quét tìm gói phản hồi 0x55
            if len(serial_buf) >= 6:
                idx = serial_buf.find(0x55)
                if idx != -1:
                    serial_buf = serial_buf[idx:]
                    if len(serial_buf) >= 6:
                        status = serial_buf[1]
                        status_word = bytes([status, 0, 0, 0])
                        expected_crc = crc32_stm32(status_word)
                        received_crc = int.from_bytes(serial_buf[2:6], byteorder='big')
                        if received_crc == expected_crc:
                            resp_status = status
                            connected = True
                            break
                        else:
                            serial_buf = serial_buf[1:]
                            
            # Quét tìm chuỗi thông báo từ STM32
            if b"Dang xoa Flash" in serial_buf or b"Phat hien lenh FOTA" in serial_buf:
                connected = True
                break
                
        if connected:
            break
            
        time.sleep(0.05)
        
    if connected:
        # Nếu chưa nhận được gói ACK thực tế (vì STM32 vẫn đang xóa Flash)
        if resp_status is None:
            print("\n[INFO] Đã kết nối với STM32! Đang chờ quá trình xóa Flash hoàn tất (khoảng 8 giây)...")
            resp_status = wait_for_response(ser, timeout_sec=15.0)
            
        if resp_status == 0x00:
            print("\n[SUCCESS] Kết nối và xóa Flash thành công! Bắt đầu truyền dữ liệu...")
        else:
            print(f"\n[ERROR] Lệnh FOTA_START bị từ chối hoặc lỗi: 0x{resp_status:02X}")
            ser.close()
            sys.exit(1)
    else:
        print("\n[ERROR] Không thể kết nối với Bootloader! STM32 không phản hồi. Huỷ bỏ.")
        ser.close()
        sys.exit(1)

    # 2. Truyền các chunk dữ liệu
    sent_bytes = 0
    chunk_index = 0
    total_chunks = (fw_size + args.chunk - 1) // args.chunk
    
    time_start = time.time()
    
    while sent_bytes < fw_size:
        chunk = firmware_data[sent_bytes : sent_bytes + args.chunk]
        retry_count = 0
        success = False
        
        while retry_count < 3 and not success:
            send_fota_packet(ser, 0x02, chunk)
            resp_status = wait_for_response(ser)
            
            if resp_status == 0x00:
                success = True
                sent_bytes += len(chunk)
                show_progress(sent_bytes, fw_size)
            else:
                retry_count += 1
                print(f"\n[WARN] Truyền chunk {chunk_index} lỗi. Trạng thái phản hồi: {resp_status}. Đang thử lại ({retry_count}/3)...")
                time.sleep(0.1)
                
        if not success:
            print(f"\n[ERROR] Lỗi truyền dữ liệu tại chunk {chunk_index} sau 3 lần thử lại! Huỷ bỏ.")
            ser.close()
            sys.exit(1)
            
        chunk_index += 1

    time_elapsed = time.time() - time_start
    print(f"\n[SUCCESS] Đã truyền xong toàn bộ dữ liệu trong {time_elapsed:.2f}s (Tốc độ ~{fw_size/time_elapsed/1024:.2f} KB/s).")

    # 3. Gửi lệnh kết thúc FOTA_CMD_END
    print("[FOTA] Đang yêu cầu STM32 xác thực toàn bộ Flash...")
    send_fota_packet(ser, 0x03, b"")
    
    resp_status = wait_for_response(ser, timeout_sec=5.0) # Tăng timeout xác thực
    if resp_status == 0x00:
        print("[SUCCESS] STM32 thông báo: Xác thực CRC32 thành công, ghi AppHeader hoàn tất và đang reset!")
    else:
        print(f"[ERROR] STM32 báo lỗi xác thực hoặc kết thúc: 0x{resp_status if resp_status is not None else 'TIMEOUT'}")
        ser.close()
        sys.exit(1)

    print("=" * 50)
    print("            CẬP NHẬT FOTA THÀNH CÔNG!")
    print("=" * 50)
    ser.close()

if __name__ == "__main__":
    main()
