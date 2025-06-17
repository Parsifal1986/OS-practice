// TempRuntimePkg/TempRuntimeDxe/TempTable.h
#ifndef _TEMP_TABLE_H_
#define _TEMP_TABLE_H_

#include <IndustryStandard/Acpi.h> // 提供 EFI_ACPI_DESCRIPTION_HEADER

#pragma pack(1)
typedef struct
{
  EFI_ACPI_DESCRIPTION_HEADER Header;
  UINT32 Temperature; // 单位：摄氏度
} ACPI_TABLE_TEMP;
#pragma pack()

#define TEMP_TABLE_SIGNATURE SIGNATURE_32('T', 'E', 'M', 'P')
#define CPU_TEMP_C 42U // 模拟温度，可按需修改

#endif
