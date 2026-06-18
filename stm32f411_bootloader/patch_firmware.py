#!/usr/bin/env python3
import os
import struct
import sys

def crc32_stm32(data: bytes) -> int:
    """
    Tính toán CRC32 theo thuật toán phần cứng của STM32 (MPEG-2 CRC32).
    Xử lý theo từng word 32-bit (little-endian).
    """
    crc = 0xFFFFFFFF
    poly = 0x04C11DB7
    
    for i in range(0, len(data), 4):
        word_bytes = data[i:i+4]
        # Pad thêm 0xFF nếu byte cuối không đủ 1 word (4 bytes)
        if len(word_bytes) < 4:
            word_bytes = word_bytes + b'\xFF' * (4 - len(word_bytes))
        
        # Đọc 32-bit word ở dạng Little Endian (khớp với cách đọc Flash của STM32)
        word = int.from_bytes(word_bytes, byteorder='little')
        
        crc ^= word
        for _ in range(32):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ poly) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc

def patch_bin(bin_path):
    if not os.path.exists(bin_path):
        print(f"Error: {bin_path} không tồn tại!")
        return False
        
    with open(bin_path, 'rb') as f:
        content = bytearray(f.read())
        
    if len(content) < 1024:
        print("Error: Kích thước file nhị phân quá nhỏ (nhỏ hơn 1KB header)!")
        return False
        
    # Dữ liệu thực tế của Application bắt đầu từ offset 1024 (sau phân vùng Header)
    app_data = content[1024:]
    firmware_size = len(app_data)
    crc_val = crc32_stm32(app_data)
    
    print(f"Patching {bin_path}:")
    print(f"  Kích thước ứng dụng: {firmware_size} bytes")
    print(f"  CRC32 phần cứng STM32 tính toán: 0x{crc_val:08X}")
    
    # Ghi kích thước firmware vào offset 4 (uint32_t, little-endian)
    struct.pack_into('<I', content, 4, firmware_size)
    # Ghi mã CRC32 tính toán được vào offset 8 (uint32_t, little-endian)
    struct.pack_into('<I', content, 8, crc_val)
    
    with open(bin_path, 'wb') as f:
        f.write(content)
    print("  Patch thành công!")
    return True

if __name__ == '__main__':
    # Đường dẫn mặc định đến file DUMMY.bin
    target_bin = 'DUMMY_APP/DUMMY/build/Debug/DUMMY.bin'
    if len(sys.argv) > 1:
        target_bin = sys.argv[1]
    
    if not patch_bin(target_bin):
        sys.exit(1)
