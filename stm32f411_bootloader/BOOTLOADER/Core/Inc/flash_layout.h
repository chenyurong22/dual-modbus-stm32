#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

/* ===================================================================================
 * BẢN ĐỒ BỘ NHỚ FLASH CHO STM32F411RET6 (Tổng 512KB)
 *  Reference Manual RM0383 (Trang 38 - Flash module organization)
 * 
 * - Sector 0: 16 KB (0x0800 0000 - 0x0800 3FFF)
 * - Sector 1: 16 KB (0x0800 4000 - 0x0800 7FFF)
 * - Sector 2: 16 KB (0x0800 8000 - 0x0800 BFFF)
 * - Sector 3: 16 KB (0x0800 C000 - 0x0800 FFFF)
 * - Sector 4: 64 KB (0x0801 0000 - 0x0801 FFFF)
 * - Sector 5-7: 128 KB mỗi Sector
 * =================================================================================== */

/* Vùng Bootloader: Chiếm Sector 0 và Sector 1 (Tổng 32KB) */
#define BL_START_ADDR        0x08000000  

/* Vùng App Header: Nằm ở đầu Sector 2 (1KB đầu tiên)
 * Chứa thông tin: Firmware Size, CRC32, Version... */
#define APP_HEADER_ADDR      0x08008000  

/* Vùng Application Code: Nằm ngay sau App Header trong Sector 2
 * Offset 0x400 (1024 bytes) chia hết cho 512, thoả mãn điều kiện dịch VTOR của Cortex-M4. */
#define APP_START_ADDR       0x08008400  

#endif /* FLASH_LAYOUT_H */
