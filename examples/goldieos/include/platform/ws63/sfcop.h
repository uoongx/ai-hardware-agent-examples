#ifndef WA63_SFCOP_H
#define WA63_SFCOP_H
#define APP_PART_START 0x003a0000
#define APP_PART_SIZE 0x00043000
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
typedef unsigned int  uint32_t;
typedef unsigned char  uint8_t;

uint32_t uapi_sfc_reg_read(uint32_t flash_addr, uint8_t *read_buffer, uint32_t read_size);
uint32_t uapi_sfc_reg_write(uint32_t flash_addr, uint8_t *write_data, uint32_t write_size);
uint32_t uapi_sfc_reg_erase(uint32_t flash_addr, uint32_t erase_size);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif