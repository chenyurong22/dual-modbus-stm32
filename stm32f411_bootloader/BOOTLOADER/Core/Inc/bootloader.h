/**
 * @file    bootloader.h
 * @brief   Header file cho Bootloader Core Logic
 *          Chứa các định nghĩa cấu trúc AppHeader và nguyên mẫu hàm.
 */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include "main.h"
#include "flash_layout.h"
#include <stdint.h>
#include <stdbool.h>

/* Magic Word để xác nhận Header hợp lệ */
#define APP_HEADER_MAGIC_WORD  0xDEADBEEF

/**
 * @brief Cấu trúc App Header đặt tại đầu Sector 2 (APP_HEADER_ADDR)
 */
typedef struct {
    uint32_t MagicWord;      /*!< Phải bằng 0xDEADBEEF */
    uint32_t FirmwareSize;   /*!< Kích thước của Application (Byte) */
    uint32_t FirmwareCrc32;  /*!< Mã CRC32 của toàn bộ Application */
    uint32_t Version;        /*!< Phiên bản Firmware */
} AppHeader_t;

/* Khai báo các hàm giao tiếp bên ngoài */

#define RING_BUF_SIZE 1024

typedef struct {
    uint8_t buffer[RING_BUF_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} RingBuffer_t;

/* Khai báo Ring Buffer toàn cục cho cổng UART FOTA */
extern RingBuffer_t g_rx_ring_buffer;
extern UART_HandleTypeDef *g_fota_uart;


/* Các hàm thao tác Ring Buffer */
void RingBuffer_Init(RingBuffer_t *rb);
void RingBuffer_Push(RingBuffer_t *rb, uint8_t byte);
bool RingBuffer_Pop(RingBuffer_t *rb, uint8_t *byte);
uint32_t RingBuffer_GetAvailable(RingBuffer_t *rb);

/* Định nghĩa giao thức FOTA */
#define FOTA_START_BYTE     0xAA
#define FOTA_RESP_START     0x55

/* Mã lệnh CMD */
#define FOTA_CMD_START      0x01
#define FOTA_CMD_WRITE      0x02
#define FOTA_CMD_END        0x03

/* Mã phản hồi STATUS */
#define FOTA_STATUS_ACK     0x00
#define FOTA_STATUS_NACK    0x01
#define FOTA_STATUS_ERR     0x02

/* Các hàm xử lý FOTA */
void FOTA_Init(void);
void FOTA_Process(void);

/**
 * @brief Kiểm tra tính hợp lệ của Application thông qua App Header và CRC.
 * @retval true nếu App hợp lệ, false nếu lỗi/không có App.
 */
bool Bootloader_VerifyApplication(void);

/**
 * @brief Vô hiệu hóa hệ thống Bootloader và nhảy sang Application.
 * @note  Hàm này sẽ không bao giờ return nếu thực thi thành công.
 */
void Bootloader_JumpToApp(void);

#endif /* BOOTLOADER_H */
