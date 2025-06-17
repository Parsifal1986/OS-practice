// TempRuntimePkg/TempRuntimeDxe/TempRuntimeDxe.c
#include "TempTable.h"

#include <Uefi.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Protocol/AcpiTable.h>

//
// 简易 8-bit 校验计算（ACPI 要求表字节和 = 0）
//
STATIC
UINT8
CalculateChecksum8 (
  CONST UINT8 *Buffer,
  UINTN        Length
  )
{
  UINTN Index;
  UINT8 Sum = 0;

  for (Index = 0; Index < Length; Index++) {
    Sum = (UINT8)(Sum + Buffer[Index]);
  }
  return (UINT8)(0 - Sum);
}

EFI_STATUS
EFIAPI
TempRuntimeDxeEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS                Status;
  EFI_ACPI_TABLE_PROTOCOL  *AcpiTableProtocol;
  ACPI_TABLE_TEMP          *TempTable;
  UINTN                     TableKey;

  //
  // 1. 获取 EFI_ACPI_TABLE_PROTOCOL
  //

  DEBUG((DEBUG_INFO, "TEMP: >>> Entry\r\n"));

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL,
                                (VOID **)&AcpiTableProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TEMP: LocateProtocol failed - %r\n", Status));
    return Status;
  }

  //
  // 2. 构造 TEMP 表并填充字段
  //
  TempTable = AllocateZeroPool (sizeof (ACPI_TABLE_TEMP));
  if (TempTable == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  TempTable->Header.Signature       = TEMP_TABLE_SIGNATURE;
  TempTable->Header.Length          = sizeof (ACPI_TABLE_TEMP);
  TempTable->Header.Revision        = 1;
  TempTable->Header.Checksum        = 0;
  CopyMem (TempTable->Header.OemId,      "OEMID ", 6);    // 必须恰好 6 字节
  TempTable->Header.OemTableId      = SIGNATURE_64('O','E','M','T','E','M','P',' ');  // 必须恰好 8 字节
  TempTable->Header.OemRevision     = 0x00000001;
  TempTable->Header.CreatorId       = SIGNATURE_32('T','M','P','D'); // "TMPD"
  TempTable->Header.CreatorRevision = 0x20250613;         // 日期示例 YYYYMMDD

  TempTable->Temperature            = CPU_TEMP_C;

  //
  // 3. 计算校验
  //
  TempTable->Header.Checksum =
      CalculateChecksum8 ((UINT8 *)TempTable, TempTable->Header.Length);

  //
  // 4. 安装表
  //
  Status = AcpiTableProtocol->InstallAcpiTable (
                                AcpiTableProtocol,
                                TempTable,
                                TempTable->Header.Length,
                                &TableKey);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TEMP: InstallAcpiTable failed - %r\n", Status));
  } else {
    DEBUG ((DEBUG_INFO,  "TEMP: Table installed, key=%Lu\n", (UINT64)TableKey));
  }

  FreePool (TempTable);
  return Status;  // 驱动留在内存（DXE_RUNTIME_DRIVER）
}
