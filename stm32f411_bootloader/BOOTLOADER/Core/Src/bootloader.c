/**
 * @file    bootloader.c
 * @brief   Source file cho Bootloader Core Logic
 *          Triển khai các hàm Jump và xác thực Firmware.
 */

#include "bootloader.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern CRC_HandleTypeDef hcrc;

/* Con trỏ xác định cổng UART dùng cho FOTA (mặc định huart2) */
UART_HandleTypeDef *g_fota_uart = &huart2;


/* Định nghĩa con trỏ hàm tới Reset Handler của Application */
typedef void (*pFunction)(void);

/**
 * @brief  Xác thực Application dựa vào App Header và kiểm tra mã CRC32 phần cứng.
 * @retval true nếu hợp lệ, false nếu không
 */
bool Bootloader_VerifyApplication(void)
{
    /* Ép kiểu địa chỉ App Header về con trỏ cấu trúc AppHeader_t */
    AppHeader_t *pHeader = (AppHeader_t *)APP_HEADER_ADDR;

    /* 1. Kiểm tra Magic Word */
    if (pHeader->MagicWord != APP_HEADER_MAGIC_WORD) {
        printf("[WARN] Magic Word khong dung! Mong doi: 0x%08X, Thuc te: 0x%08X\r\n", 
               (unsigned int)APP_HEADER_MAGIC_WORD, (unsigned int)pHeader->MagicWord);
        return false;
    }

    /* 2. Kiểm tra kích thước Firmware (Chống tràn Flash) */
    if (pHeader->FirmwareSize == 0 || pHeader->FirmwareSize > (512 * 1024 - 32 * 1024)) {
        printf("[WARN] Kich thuoc firmware hop le: %lu bytes\r\n", pHeader->FirmwareSize);
        return false; /* Vượt quá giới hạn Flash (512KB - 32KB Bootloader) */
    }

    /* 3. Kiểm tra CRC32 toàn bộ vung nho Application */
    uint32_t *pData = (uint32_t *)APP_START_ADDR;
    uint32_t words_count = pHeader->FirmwareSize / 4;
    uint32_t remaining_bytes = pHeader->FirmwareSize % 4;

    /* Reset bo CRC cua MCU ve trang thai ban dau */
    __HAL_CRC_DR_RESET(&hcrc);

    /* Tinh toan CRC32 cho cac block 32-bit (word) */
    uint32_t computed_crc = HAL_CRC_Calculate(&hcrc, pData, words_count);

    /* Neu con du tu 1-3 bytes cuoi, pad them 0xFF de du 1 word va tinh not */
    if (remaining_bytes > 0) {
        uint32_t last_word = 0xFFFFFFFF;
        uint8_t *p_last_byte = (uint8_t *)&last_word;
        uint8_t *p_src = (uint8_t *)(pData + words_count);
        for (uint32_t i = 0; i < remaining_bytes; i++) {
            p_last_byte[i] = p_src[i];
        }
        computed_crc = HAL_CRC_Accumulate(&hcrc, &last_word, 1);
    }

    /* So sanh ma CRC32 tinh toan voi ma CRC32 nam trong Header */
    if (computed_crc != pHeader->FirmwareCrc32) {
        printf("[ERROR] Sai ma CRC32! Tinh duoc: 0x%08X, Header: 0x%08X\r\n", 
               (unsigned int)computed_crc, (unsigned int)pHeader->FirmwareCrc32);
        return false;
    }

    printf("[INFO] CRC32 hop le: 0x%08X. Kich thuoc: %lu bytes\r\n", 
           (unsigned int)computed_crc, pHeader->FirmwareSize);
    return true;
}

/**
 * @brief  Nhảy sang Application (Thực hiện chu trình dọn dẹp và Jump)
 * @retval None
 */
void Bootloader_JumpToApp(void)
{
    /* Đọc giá trị MSP và Reset Handler từ Vector Table của App */
    uint32_t mspAddress = *(__IO uint32_t*)APP_START_ADDR;
    uint32_t jumpAddress = *(__IO uint32_t*)(APP_START_ADDR + 4);

    /* Tắt ngắt toàn cục trước khi thực hiện dọn dẹp */
    __disable_irq();

    /* Đưa SysTick về trạng thái Reset (Tránh ngắt SysTick làm crash App) */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    /* De-Initialize các ngoại vi đã dùng trong Bootloader */
    HAL_UART_DeInit(&huart1);
    HAL_UART_DeInit(&huart2);
    HAL_CRC_DeInit(&hcrc);
    HAL_RCC_DeInit();

    /* Xóa toàn bộ các cờ ngắt đang chờ (Pending Interrupts) */
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }

    /* Đổi Vector Table Offset (VTOR) sang phân vùng của Application */
    SCB->VTOR = APP_START_ADDR;

    /* Ghi MSP, bật lại ngắt và Jump sang App bằng Inline Assembly để tránh compiler sinh code dùng Stack (gây crash ở -O0) */
    __asm volatile (
        "msr msp, %0\n\t"  /* Thiết lập MSP bằng giá trị mspAddress */
        "cpsie i\n\t"      /* Mở lại ngắt toàn cục */
        "bx %1\n\t"        /* Nhảy trực tiếp tới địa chỉ jumpAddress (Reset Handler của App) */
        :
        : "r" (mspAddress), "r" (jumpAddress)
        : "memory"
    );

    /* Không bao giờ chạy đến đây */
    while(1);
}

/* ========================================================================== */
/*                   TRIỂN KHAI RING BUFFER & GIAO THỨC FOTA                   */
/* ========================================================================== */

/* Định nghĩa Ring Buffer toàn cục */
RingBuffer_t g_rx_ring_buffer;

void RingBuffer_Init(RingBuffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

void RingBuffer_Push(RingBuffer_t *rb, uint8_t byte)
{
    uint32_t next = (rb->head + 1) % RING_BUF_SIZE;
    if (next != rb->tail) {
        rb->buffer[rb->head] = byte;
        rb->head = next;
    }
}

bool RingBuffer_Pop(RingBuffer_t *rb, uint8_t *byte)
{
    if (rb->head == rb->tail) {
        return false; // Rỗng
    }
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUF_SIZE;
    return true;
}

uint32_t RingBuffer_GetAvailable(RingBuffer_t *rb)
{
    if (rb->head >= rb->tail) {
        return rb->head - rb->tail;
    } else {
        return RING_BUF_SIZE - rb->tail + rb->head;
    }
}

/* Biến trạng thái FOTA */
typedef enum {
    FOTA_STATE_IDLE,
    FOTA_STATE_RX_HEADER,
    FOTA_STATE_RX_PAYLOAD,
    FOTA_STATE_PROCESSING
} FotaProcessState_t;

static FotaProcessState_t fota_state = FOTA_STATE_IDLE;
static uint8_t rx_cmd = 0;
static uint16_t rx_len = 0;
static uint16_t rx_idx = 0;
static uint8_t rx_payload[512];
static uint32_t rx_crc = 0;

uint32_t fota_expected_size = 0;
static uint32_t fota_expected_crc = 0;
static uint32_t fota_write_address = APP_START_ADDR;

/* Tính toán CRC32 cho một mảng bytes (STM32 MPEG-2 standard) */
static uint32_t FOTA_CalculateCRC32(uint8_t *pData, uint32_t len)
{
    __HAL_CRC_DR_RESET(&hcrc);
    uint32_t words = len / 4;
    uint32_t rem = len % 4;
    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)pData, words);
    if (rem > 0) {
        uint32_t last_word = 0xFFFFFFFF;
        uint8_t *p_last = (uint8_t *)&last_word;
        uint8_t *p_src = pData + (words * 4);
        for (uint32_t i = 0; i < rem; i++) {
            p_last[i] = p_src[i];
        }
        crc = HAL_CRC_Accumulate(&hcrc, &last_word, 1);
    }
    return crc;
}

/* Phản hồi trạng thái về Host */
static void fota_send_response(uint8_t status)
{
    uint8_t resp[6];
    resp[0] = FOTA_RESP_START;
    resp[1] = status;
    
    // Tính CRC32 của status byte (được ép kiểu thành 32-bit word)
    uint32_t word = (uint32_t)status;
    uint32_t crc = FOTA_CalculateCRC32((uint8_t*)&word, 4);
    
    resp[2] = (uint8_t)(crc >> 24);
    resp[3] = (uint8_t)(crc >> 16);
    resp[4] = (uint8_t)(crc >> 8);
    resp[5] = (uint8_t)(crc & 0xFF);
    
    HAL_UART_Transmit(g_fota_uart, resp, 6, HAL_MAX_DELAY);
}


/* Xóa các sector chứa ứng dụng (Sector 2 đến 7) */
static bool FOTA_EraseApplicationSectors(void)
{
    printf("[FOTA] Dang xoa Flash Sectors 2-7...\r\n");
    
    /* Trước hết phải Unprotect/Unlock bộ nhớ Flash */
    HAL_FLASH_Unlock();
    
    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = FLASH_SECTOR_2;
    erase_init.NbSectors = 6; // Sector 2, 3, 4, 5, 6, 7
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    uint32_t sector_error = 0;
    
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
    HAL_FLASH_Lock();
    
    if (status != HAL_OK) {
        printf("[ERROR] Xoa Flash that bai! Sector loi: %lu\r\n", sector_error);
        return false;
    }
    
    printf("[FOTA] Xoa Flash thanh cong!\r\n");
    return true;
}

/* Xử lý gói tin FOTA nhận được */
static void fota_handle_packet(void)
{
    // 1. Kiểm tra CRC32 gói tin
    static uint8_t crc_buf[512 + 4];
    crc_buf[0] = rx_cmd;
    crc_buf[1] = (uint8_t)(rx_len >> 8);
    crc_buf[2] = (uint8_t)(rx_len & 0xFF);
    memcpy(&crc_buf[3], rx_payload, rx_len);
    
    uint32_t computed_crc = FOTA_CalculateCRC32(crc_buf, 3 + rx_len);
    if (computed_crc != rx_crc) {
        printf("[ERROR] Checksum goi tin FOTA khong khop! Tinh duoc: 0x%08X, Nhan duoc: 0x%08X\r\n", 
               (unsigned int)computed_crc, (unsigned int)rx_crc);
        fota_send_response(FOTA_STATUS_NACK);
        return;
    }
    
    // 2. Phân loại lệnh
    switch (rx_cmd) {
        case FOTA_CMD_START:
            if (rx_len < 12) {
                fota_send_response(FOTA_STATUS_ERR);
                return;
            }
            fota_expected_size = *((uint32_t*)&rx_payload[0]);
            fota_expected_crc = *((uint32_t*)&rx_payload[4]);
            printf("[FOTA] Bat dau FOTA. Kich thuoc mong doi: %lu bytes, CRC32 mong doi: 0x%08X\r\n", 
                   fota_expected_size, (unsigned int)fota_expected_crc);
            
            if (!FOTA_EraseApplicationSectors()) {
                fota_send_response(FOTA_STATUS_ERR);
                return;
            }
            
            fota_write_address = APP_START_ADDR; // 0x08008400
            fota_send_response(FOTA_STATUS_ACK);
            break;
            
        case FOTA_CMD_WRITE:
            if (rx_len == 0) {
                fota_send_response(FOTA_STATUS_ACK);
                return;
            }
            if (fota_write_address + rx_len > 0x08080000) { // Vượt quá 512KB Flash
                printf("[ERROR] Ghi Flash vuot qua dung luong cho phep!\r\n");
                fota_send_response(FOTA_STATUS_ERR);
                return;
            }
            
            // Ghi dữ liệu vào Flash
            HAL_FLASH_Unlock();
            for (uint16_t i = 0; i < rx_len; i++) {
                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, fota_write_address + i, rx_payload[i]) != HAL_OK) {
                    printf("[ERROR] Ghi byte tai dia chi 0x%08X that bai!\r\n", (unsigned int)(fota_write_address + i));
                    HAL_FLASH_Lock();
                    fota_send_response(FOTA_STATUS_ERR);
                    return;
                }
            }
            HAL_FLASH_Lock();
            
            fota_write_address += rx_len;
            fota_send_response(FOTA_STATUS_ACK);
            break;
            
        case FOTA_CMD_END:
            printf("[FOTA] Nhap hoan tat file. Dang tien hanh kiem tra toan ven...\r\n");
            // Tính toán CRC32 toàn bộ file đã ghi trên Flash
            uint32_t *pData = (uint32_t *)APP_START_ADDR;
            uint32_t words_count = fota_expected_size / 4;
            uint32_t remaining_bytes = fota_expected_size % 4;

            __HAL_CRC_DR_RESET(&hcrc);
            uint32_t computed_file_crc = HAL_CRC_Calculate(&hcrc, pData, words_count);
            if (remaining_bytes > 0) {
                uint32_t last_word = 0xFFFFFFFF;
                uint8_t *p_last_byte = (uint8_t *)&last_word;
                uint8_t *p_src = (uint8_t *)(pData + words_count);
                for (uint32_t i = 0; i < remaining_bytes; i++) {
                    p_last_byte[i] = p_src[i];
                }
                computed_file_crc = HAL_CRC_Accumulate(&hcrc, &last_word, 1);
            }
            
            if (computed_file_crc != fota_expected_crc) {
                printf("[ERROR] CRC file tren Flash khong khop! Tinh duoc: 0x%08X, Mong doi: 0x%08X\r\n", 
                       (unsigned int)computed_file_crc, (unsigned int)fota_expected_crc);
                fota_send_response(FOTA_STATUS_ERR);
                return;
            }
            
            printf("[INFO] CRC File tren Flash KHOP: 0x%08X. Dang ghi AppHeader...\r\n", (unsigned int)computed_file_crc);
            
            // Ghi AppHeader để chính thức kích hoạt ứng dụng
            AppHeader_t header;
            header.MagicWord = APP_HEADER_MAGIC_WORD;
            header.FirmwareSize = fota_expected_size;
            header.FirmwareCrc32 = fota_expected_crc;
            header.Version = 1;
            
            HAL_FLASH_Unlock();
            uint8_t *p_header_bytes = (uint8_t *)&header;
            for (uint32_t i = 0; i < sizeof(AppHeader_t); i++) {
                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, APP_HEADER_ADDR + i, p_header_bytes[i]) != HAL_OK) {
                    printf("[ERROR] Ghi byte Header tai 0x%08X that bai!\r\n", (unsigned int)(APP_HEADER_ADDR + i));
                    HAL_FLASH_Lock();
                    fota_send_response(FOTA_STATUS_ERR);
                    return;
                }
            }
            HAL_FLASH_Lock();
            
            printf("[INFO] FOTA thanh cong! Reset lai vi dieu khien...\r\n");
            fota_send_response(FOTA_STATUS_ACK);
            
            HAL_Delay(500);
            HAL_NVIC_SystemReset(); // Reset MCU để khởi chạy App mới thông qua quy trình Boot thông thường
            break;
            
        default:
            fota_send_response(FOTA_STATUS_ERR);
            break;
    }
}

void FOTA_Init(void)
{
    RingBuffer_Init(&g_rx_ring_buffer);
    fota_state = FOTA_STATE_IDLE;
    
    // Kích hoạt NVIC IRQ cho USART2 và USART1
    HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    
    // Bật ngắt RXNE cho cả hai USART
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    
    printf("[FOTA] Da bat ngat USART1 & USART2 RX RingBuffer.\r\n");
}


void FOTA_Process(void)
{
    uint8_t byte;
    while (RingBuffer_Pop(&g_rx_ring_buffer, &byte)) {
        printf("[RX] 0x%02X\r\n", byte);
        switch (fota_state) {
            case FOTA_STATE_IDLE:
                if (byte == FOTA_START_BYTE) {
                    fota_state = FOTA_STATE_RX_HEADER;
                    rx_idx = 0;
                }
                break;
                
            case FOTA_STATE_RX_HEADER:
                if (rx_idx == 0) {
                    rx_cmd = byte;
                    rx_idx++;
                } else if (rx_idx == 1) {
                    rx_len = ((uint16_t)byte) << 8;
                    rx_idx++;
                } else if (rx_idx == 2) {
                    rx_len |= byte;
                    if (rx_len > sizeof(rx_payload)) {
                        fota_send_response(FOTA_STATUS_NACK);
                        fota_state = FOTA_STATE_IDLE;
                    } else {
                        if (rx_len > 0) {
                            fota_state = FOTA_STATE_RX_PAYLOAD;
                            rx_idx = 0;
                        } else {
                            fota_state = FOTA_STATE_PROCESSING;
                            rx_idx = 0;
                        }
                    }
                }
                break;
                
            case FOTA_STATE_RX_PAYLOAD:
                if (rx_idx < rx_len) {
                    rx_payload[rx_idx] = byte;
                    rx_idx++;
                } else {
                    uint32_t crc_shift = rx_idx - rx_len;
                    if (crc_shift == 0) {
                        rx_crc = ((uint32_t)byte) << 24;
                    } else if (crc_shift == 1) {
                        rx_crc |= ((uint32_t)byte) << 16;
                    } else if (crc_shift == 2) {
                        rx_crc |= ((uint32_t)byte) << 8;
                    } else if (crc_shift == 3) {
                        rx_crc |= byte;
                        fota_state = FOTA_STATE_PROCESSING;
                    }
                    rx_idx++;
                }
                break;
                
            default:
                break;
        }
        
        if (fota_state == FOTA_STATE_PROCESSING) {
            fota_handle_packet();
            fota_state = FOTA_STATE_IDLE;
        }
    }
}

/**
 * @brief Xử lý ngắt USART2 (Bao gồm nhận dữ liệu qua RXNE và xử lý lỗi ORE)
 */
void USART2_IRQHandler(void)
{
    uint32_t sr = USART2->SR;
    
    // Nếu có dữ liệu nhận (RXNE) hoặc bất kỳ cờ lỗi nào (ORE, FE, NE, PE) được set
    if (sr & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        // Đọc DR một lần duy nhất để lấy dữ liệu và tự động xóa toàn bộ các cờ lỗi trên
        uint8_t byte = (uint8_t)(USART2->DR & 0xFF);
        
        // Chỉ đẩy vào Ring Buffer nếu thực sự có byte nhận hợp lệ
        if (sr & USART_SR_RXNE) {
            g_fota_uart = &huart2;
            RingBuffer_Push(&g_rx_ring_buffer, byte);
        }
    }
}

