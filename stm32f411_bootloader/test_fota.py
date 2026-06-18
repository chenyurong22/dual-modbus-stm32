#!/usr/bin/env python3
import sys
import os
import time
import serial

# Sử dụng cùng thuật toán CRC32 phần cứng của STM32
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
    print(f"-> Gửi gói tin CMD=0x{cmd:02X}, LEN={payload_len}: {packet.hex().upper()}")
    ser.write(packet)

def main():
    port = "/dev/ttyUSB0"
    baud = 115200
    
    print(f"Đang kết nối tới cổng {port} ở tốc độ {baud}...")
    try:
        ser = serial.Serial(port, baud, timeout=0.5)
    except Exception as e:
        print(f"Lỗi kết nối cổng serial: {e}")
        return
        
    print("Mẹo: Reset mạch STM32 để xem luồng Bootloader chạy.")
    print("Nhấn Ctrl+C để thoát.")
    
    # Tạo payload giả lập cho lệnh START_UPDATE
    # Kích thước: 16000 bytes (0x3E80)
    # CRC32 mong đợi: 0x5D6F9995
    # Version: 1
    size_bytes = (16000).to_bytes(4, byteorder='little')
    crc_bytes = (0x5D6F9995).to_bytes(4, byteorder='little')
    version_bytes = (1).to_bytes(4, byteorder='little')
    start_payload = size_bytes + crc_bytes + version_bytes
    
    sent_start = False
    
    try:
        while True:
            # Đọc log debug từ STM32
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                try:
                    text = data.decode('utf-8', errors='ignore')
                    print(text, end='', flush=True)
                    
                    # Nếu thấy Bootloader bắt đầu đếm ngược, gửi lệnh START_UPDATE ngay lập tức
                    if "Starting 3s FOTA countdown" in text and not sent_start:
                        print("\n[PC] Phát hiện đếm ngược! Gửi lệnh START_UPDATE...")
                        time.sleep(0.1) # Đợi chút để UART ổn định
                        send_fota_packet(ser, 0x01, start_payload)
                        sent_start = True
                        
                except Exception as e:
                    print(f"\n[Lỗi hiển thị dữ liệu]: {e}")
            
            # Đọc phản hồi của FOTA lệnh nếu có
            if sent_start and ser.in_waiting >= 6:
                resp = ser.read(6)
                print(f"\n<- Nhận phản hồi FOTA từ STM32: {resp.hex().upper()}")
                if resp[0] == 0x55:
                    status = resp[1]
                    if status == 0x00:
                        print("[SUCCESS] Nhận ACK thành công! STM32 đã xóa Flash và sẵn sàng nhận Chunks.")
                    else:
                        print(f"[ERROR] Nhận NACK hoặc lỗi: 0x{status:02X}")
                sent_start = False
                
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("\nĐã thoát.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
