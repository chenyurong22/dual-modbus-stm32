# Hướng dẫn cấu hình STM32CubeMX cho Bootloader (STM32F411RET6)

Để xây dựng Bootloader tích hợp Modbus RTU, bạn cần cấu hình STM32CubeMX theo các bước chuẩn xác dưới đây. Sau khi tạo project mới và chọn chip **STM32F411RET6**, hãy làm lần lượt:

## 1. System Core (Cấu hình hệ thống)
* **SYS:** 
  * Debug: Chọn **Serial Wire** (Bắt buộc để nạp và debug qua ST-Link).
  * Timebase Source: Để mặc định là **SysTick** (Do Bootloader chạy bare-metal, không dùng RTOS).
* **RCC (Xung nhịp):** 
  * High Speed Clock (HSE): Nếu mạch bạn có thạch anh ngoài thì chọn **Crystal/Ceramic Resonator**. Nếu mạch tự hàn không có thạch anh, hãy để Disable (Chip sẽ dùng dao động nội HSI).
* **GPIO (Chân điều khiển):**
  * Chọn chân **PA5** (Hoặc chân nối với LED trên mạch của bạn), cấu hình là **GPIO_Output**. Đặt User Label là `LED_STATUS`. (Dùng để nháy báo hiệu đang trong Bootloader). CHON LED LD4 PD12 
## 2. Computing (Tính toán phần cứng)
* **CRC:** 
  * Tích chọn **Activated**.
  * Chức năng này sẽ kích hoạt bộ Hardware CRC mặc định của STM32F4 (tính mã **CRC32**). Ta sẽ dùng bộ CRC32 siêu tốc này để kiểm tra tính toàn vẹn của từng gói tin UART gửi từ PC và dùng để xác thực toàn bộ file Firmware (bằng cách so sánh với CRC32 lưu trong AppHeader).

## 3. Connectivity (Giao tiếp)
* **USART1 (Dùng cho giao tiếp FOTA với PC):**
  * Mode: **Asynchronous**
  * Hardware Flow Control: Disable
  * **Parameter Settings:** 
    * Baud Rate: `115200`.
    * Word Length: `8 Bits`
    * Parity: `None`
    * Stop Bits: `1`
  * **NVIC Settings:** Tích chọn **USART1 global interrupt** (Rất quan trọng: ta sẽ dùng ngắt để nhận chuỗi dữ liệu Firmware từ máy tính).
  * Chân mặc định sẽ là `PA9` (TX) và `PA10` (RX).

* **USART2 (Dùng để in Log Debug):**
  * Mode: **Asynchronous**
  * Parameter Settings: `115200`, 8 Bits, None, 1 Stop Bit.
  * Chân mặc định thường là `PA2` (TX) và `PA3` (RX) - Nếu bạn dùng board Nucleo thì 2 chân này nối thẳng với cổng USB máy tính.

## 4. Clock Configuration (Cây xung nhịp)
* Tùy thuộc vào thiết lập RCC ở bước 1, bạn chuyển sang tab Clock và cấu hình **HCLK (MHz)** lên mức tối đa là **100 MHz** rồi nhấn Enter để CubeMX tự tính toán các bộ chia.

## 5. Project Manager (Xuất code)
* **Project Name:** `Main_Bootloader`
* **Project Location:** Trỏ vào thư mục `stm32f411_bootloader` của chúng ta.
* **Toolchain / IDE:** Chọn `STM32CubeIDE` (Hoặc `Makefile` nếu bạn build bằng lệnh gcc).
* **Code Generator (Tab):** 
  * Tích chọn: *"Generate peripheral initialization as a pair of .c/.h files per peripheral"* (Để code sinh ra được chia thành `usart.c`, `gpio.c` riêng biệt cho sạch sẽ, tuân thủ đúng luật `GEMINI.md`).
  
**==> Cuối cùng:** Nhấn **GENERATE CODE**!
