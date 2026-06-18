# TÀI LIỆU HƯỚNG DẪN AI TRỢ LÝ (GEMINI AGENT GUIDELINES)

Tài liệu này định nghĩa các quy tắc bắt buộc, nghiêm ngặt dành cho AI (Agent) khi tham gia phát triển dự án Bootloader cho STM32F411RET6. Mọi dòng code, mọi phản hồi đều phải tuân thủ tuyệt đối các tiêu chuẩn dưới đây.

---

## 1. PHƯƠNG PHÁP CODE AN TOÀN (SAFE CODING & TESTING)
- **Kiểm tra đầu vào (Defensive Programming):** Luôn kiểm tra các con trỏ (pointer) `NULL`, tham số đầu vào của hàm và các giới hạn mảng trước khi thao tác.
- **Testing & Debugging:** 
  - Code sinh ra phải đi kèm với phương pháp test cụ thể (ví dụ: in ra UART, chớp LED báo lỗi, hoặc các hàm Assert).
  - Bắt buộc phải có cơ chế xử lý lỗi (Error Handling). Không bao giờ để hệ thống bị treo cứng (HardFault) mà không có cảnh báo.
  - Sử dụng `#define DEBUG` và các macro logging để hỗ trợ quá trình debug.

## 2. TIÊU CHUẨN LẬP TRÌNH NHÚNG (EMBEDDED CODING STANDARDS)
- **Sạch sẽ và ngắn gọn:** Code phải tối ưu, dễ đọc, không rườm rà. Mỗi hàm chỉ nên thực hiện một nhiệm vụ duy nhất.
- **Tiêu chuẩn đặt tên:** Thống nhất cách đặt tên biến, hàm (ví dụ: `snake_case` cho biến/hàm, `UPPER_CASE` cho macro/hằng số). Tiền tố rõ ràng cho từng module (VD: `BOOT_`, `FLASH_`, `UART_`).
- **Tránh cấp phát động:** Tuyệt đối KHÔNG sử dụng `malloc()`, `free()` hay cấp phát động trong Bootloader để tránh phân mảnh bộ nhớ.
- **Nguyên tắc MISRA C:** Ưu tiên tuân thủ các nguyên tắc MISRA C cơ bản để đảm bảo tính an toàn cho hệ thống nhúng.

## 3. QUY TẮC COMMENT & TÀI LIỆU (DOCUMENTATION)
- **100% Tiếng Việt:** Tất cả các comment giải thích code, đặc biệt là phần mô tả hàm, đều bắt buộc PHẢI viết bằng Tiếng Việt.
- **Chuẩn Doxygen:** Sử dụng định dạng Doxygen cho các header của hàm:
  ```c
  /**
   * @brief  Mô tả ngắn gọn chức năng của hàm.
   * @param  Tên_tham_số: Giải thích ý nghĩa.
   * @retval Giá trị trả về và ý nghĩa.
   */
  ```
- Code phải tự giải thích được ý nghĩa (self-explanatory code), comment dùng để giải thích *TẠI SAO* lại làm như vậy thay vì *LÀM GÌ*.

## 4. TƯƠNG TÁC VỚI PHẦN CỨNG (HARDWARE INTERACTION)
- **KHÔNG ĐOÁN MÒ:** Nếu có bất kỳ sự thiếu chắc chắn nào về cấu hình phần cứng (Clock tree, Pinout, GPIO mode, Thanh ghi, Ngắt), Agent **PHẢI DỪNG LẠI VÀ HỎI USER**.
- **Xác nhận cấu hình:** Luôn yêu cầu user xác nhận mạch nguyên lý (Schematic) hoặc file `.ioc` (STM32CubeMX) trước khi viết code giao tiếp ngoại vi.
- **Quản lý bộ nhớ (Memory Layout):** Việc phân chia bộ nhớ (Sector, Address) giữa Bootloader và Application phải được xác định và ghi chú rõ ràng bằng Macro.

---

## CÁC YÊU CẦU CẦN THIẾT ĐỂ THỰC HIỆN DỰ ÁN NÀY (Dành cho User & Agent)

Để dự án Bootloader diễn ra chuyên nghiệp và trơn tru, chúng ta cần xác định và bổ sung các thông tin sau vào dự án:

1. **Phân bổ bộ nhớ (Memory Map):**
   - Địa chỉ bắt đầu của Bootloader (thường là `0x08000000`).
   - Địa chỉ bắt đầu của Application (ví dụ: `0x08008000`).
   - Vùng nhớ dùng để chia sẻ dữ liệu giữa Bootloader và App (nếu có).
2. **Giao thức cập nhật (Update Protocol):**
   - Nguồn nhận Firmware: Qua UART (Ymodem?), USB (DFU?), hay đọc từ thẻ nhớ/bộ nhớ ngoài (SPI Flash)?
   - Cấu trúc bản tin (Frame format) nếu dùng giao thức custom.
3. **Cơ chế xác thực Firmware (Validation):**
   - Sử dụng CRC32, Checksum, hay chữ ký số (Digital Signature) để kiểm tra tính toàn vẹn của Firmware trước khi Jump?
4. **Cơ chế Jump to Application:**
   - Cấu hình lại Stack Pointer (MSP).
   - Relocate Vector Table (VTOR).
   - De-initialize (vô hiệu hóa) các ngoại vi và ngắt (Interrupts) đã dùng trong Bootloader trước khi nhảy sang App.
5. **Kịch bản rủi ro (Fallback Strategy):**
   - Xử lý thế nào nếu quá trình cập nhật bị mất điện giữa chừng?
   - Cần có cơ chế Rollback hoặc lưu bản backup của Firmware cũ hay không?

*Bằng việc tuân thủ nghiêm ngặt tài liệu này, hệ thống Bootloader sẽ đạt được độ tin cậy và an toàn cao nhất.*
