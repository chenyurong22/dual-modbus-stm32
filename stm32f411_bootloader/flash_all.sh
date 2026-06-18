#!/bin/bash
# Ngăn script tiếp tục chạy nếu có lệnh bị lỗi
set -e

echo "==========================================="
echo "  BIÊN DỊCH VÀ NẠP BOOTLOADER + DUMMY APP  "
echo "==========================================="

# Xác định thư mục của script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

echo "[1/3] Đang biên dịch Bootloader..."
cd "$SCRIPT_DIR/BOOTLOADER"
cmake --build build/Debug

echo "[2/3] Đang biên dịch Dummy App..."
cd "$SCRIPT_DIR/DUMMY_APP/DUMMY"
cmake --build build/Debug

# Tự động vá kích thước thực tế và mã CRC32 phần cứng vào Header của Dummy App
python3 "$SCRIPT_DIR/patch_firmware.py" "$SCRIPT_DIR/DUMMY_APP/DUMMY/build/Debug/DUMMY.bin"

echo "[3/3] Đang nạp cả hai tệp BIN xuống STM32..."
# Lệnh OpenOCD:
# - init: khởi tạo kết nối
# - reset halt: reset chip và dừng core
# - flash erase_sector 0 2 7: xóa sạch phân vùng ứng dụng (Sector 2 đến 7)
# - program BOOTLOADER.bin 0x08000000: nạp phân đoạn bootloader (Sector 0-1)
# - program DUMMY.bin 0x08008000: nạp phân đoạn ứng dụng (Sector 2 trở đi) và chạy
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
  -c "init" \
  -c "reset halt" \
  -c "flash erase_sector 0 2 7" \
  -c "program $SCRIPT_DIR/BOOTLOADER/build/Debug/BOOTLOADER.bin 0x08000000 verify" \
  -c "program $SCRIPT_DIR/DUMMY_APP/DUMMY/build/Debug/DUMMY.bin 0x08008000 verify reset exit"

echo "==========================================="
echo "   NẠP THÀNH CÔNG! ĐANG KHỞI ĐỘNG MẠCH...  "
echo "==========================================="
