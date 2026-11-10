#include "stm32f4xx.h"
typedef enum {FLASH_COMPLETE, FLASH_ERROR_PROGRAM} FLASH_Status;
#define FLASH_Sector_3 3
#define FLASH_Sector_4 4
#define FLASH_Sector_5 5
#define FLASH_Sector_6 6
#define FLASH_Sector_7 7
#define VoltageRange_3 3
#define FLASH_FLAG_EOP 1
#define FLASH_FLAG_OPERR 2
#define FLASH_FLAG_WRPERR 4
#define FLASH_FLAG_PGAERR 8
#define FLASH_FLAG_PGPERR 16
#define FLASH_FLAG_PGSERR 32
void FLASH_Unlock(void);
void FLASH_Lock(void);
void FLASH_ClearFlag(uint32_t flags);
FLASH_Status FLASH_EraseSector(uint32_t sector, uint8_t voltage);
FLASH_Status FLASH_ProgramByte(uint32_t address, uint8_t data);
FLASH_Status FLASH_ProgramWord(uint32_t address, uint32_t data);
void FLASH_DataCacheCmd(FunctionalState state);
void FLASH_InstructionCacheCmd(FunctionalState state);
void FLASH_DataCacheReset(void);
void FLASH_InstructionCacheReset(void);
