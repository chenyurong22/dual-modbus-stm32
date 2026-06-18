# Main Bootloader Workspace

Thư mục này là nơi chứa source code chính cho dự án Bootloader của STM32F411RET6.
Tất cả code tự viết, hoặc code sinh ra từ STM32CubeMX sẽ được đặt tại đây.

## Cấu trúc dự kiến:
- `Core/Src/bootloader.c` : Chứa logic chính của Bootloader (xử lý UART, Flash, Jump to App).
- `Core/Inc/bootloader.h` : Chứa các định nghĩa Macro (Memory Map, Command) và khai báo hàm.
