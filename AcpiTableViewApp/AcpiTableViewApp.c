#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/AcpiViewCommandLib.h>

#include <Protocol/ShellParameters.h>

typedef char* TABLE_NAME;
typedef UINT32 SIGNATURE_BYTE;
typedef UINT32 TABLE_LENGTH_BYTE;
typedef UINT32 TABLE_LENGTH_OFFSET;
typedef UINT32 OEMID_BYTE;
typedef UINT32 OEMID_OFFSET;
typedef UINT32 CHECKSUM_BYTE;
typedef UINT32 CHECKSUM_OFFSET;

STATIC UINT32 mTableCount;
STATIC UINT32 mBinTableCount;

BOOLEAN
EFIAPI
MyCompareGuid(
    IN CONST GUID *Guid1,
    IN CONST GUID *Guid2)
{
  UINT64 LowPartOfGuid1;
  UINT64 LowPartOfGuid2;
  UINT64 HighPartOfGuid1;
  UINT64 HighPartOfGuid2;

  LowPartOfGuid1 = ReadUnaligned64((CONST UINT64 *)Guid1);
  LowPartOfGuid2 = ReadUnaligned64((CONST UINT64 *)Guid2);
  HighPartOfGuid1 = ReadUnaligned64((CONST UINT64 *)Guid1 + 1);
  HighPartOfGuid2 = ReadUnaligned64((CONST UINT64 *)Guid2 + 1);

  return (BOOLEAN)(LowPartOfGuid1 == LowPartOfGuid2 && HighPartOfGuid1 == HighPartOfGuid2);
}

VOID
PrintString(
  IN char *String,
  IN INT32 Length
) {
  UINT32 i;
  for (i = 0; String[i] != '\0' && i < Length; i++) {
    Print(L"%c", String[i]);
  }
}

VOID
EFIAPI
PrintTable(
  IN TABLE_NAME TableName,
  IN UINT8 *TablePtr,
  IN SIGNATURE_BYTE SignatureByte,
  IN TABLE_LENGTH_BYTE TableLengthByte,
  IN TABLE_LENGTH_OFFSET TableLengthOffset,
  IN OEMID_BYTE OemIdByte,
  IN OEMID_OFFSET OemIdOffset,
  IN CHECKSUM_BYTE ChecksumByte,
  IN CHECKSUM_OFFSET ChecksumOffset
) {
  UINTN oemidreadingoffset;

  if (TablePtr == NULL) {
    if (TableName != NULL) {
      Print(L"ERROR: ");
      PrintString(TableName, SignatureByte);
      Print(L" Table Pointer is NULL.\n");
    } else {
      Print(L"ERROR: Table Pointer is NULL.\n");
    }
    return;
  }

  if (TableName == NULL) {
    TableName = (char*)TablePtr;
  }

  Print(L"=====");
  PrintString(TableName, SignatureByte);
  Print(L" Table=====\n");
  // Print(L"=====%s Table=====\n", TableName);
  Print(L"Physical Address: 0x%p\n", (TablePtr));
  Print(L"Table Length: %u\n", *(UINT32 *)(TablePtr + TableLengthOffset));
  Print(L"OEMID: ");
  for (oemidreadingoffset = 0; oemidreadingoffset < OemIdByte; oemidreadingoffset++) {
    Print(L"%c", *(TablePtr + oemidreadingoffset + OemIdOffset));
  }
  Print(L"\n");
  Print(L"checksum: 0x%02x\n", *(UINT8 *)(TablePtr + ChecksumOffset));
  Print(L"\n");
}

VOID
EFIAPI
ChangeTable(
  IN UINT8 *TablePtr
) {
  UINTN oemidreadingoffset;
  char NewOemId[6] = "NEWID "; // New OEMID to set
  UINT8 NewCheckSum = 0;
  UINTN i;

  for (oemidreadingoffset = 0; oemidreadingoffset < 6; oemidreadingoffset++)
  {
    *(TablePtr + 10 + oemidreadingoffset) = NewOemId[oemidreadingoffset];
  }
  for (i = 0; i < *(UINT32*)(TablePtr + 4); i++) {
    if (i != 9) {
      NewCheckSum += *(TablePtr + i);
    }
  }
  *(TablePtr + 9) = 0 - NewCheckSum;
}

EFI_STATUS
EFIAPI
PrintAllACPITables(
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  SHELL_STATUS ShellStatus;
  UINTN Index;
  BOOLEAN FoundAcpiTable;
  // EREPORT_OPTION ReportOption;
  UINT8 *RsdpPtr;
  // UINT32 RsdpLength;
  UINT8 RsdpRevision;
  // PARSE_ACPI_TABLE_PROC RsdpParserProc;
  // BOOLEAN Trace;

  mTableCount = 0;
  mBinTableCount = 0;

  if (SystemTable == NULL) {
    ShellStatus = SHELL_INVALID_PARAMETER;
  } else {
    FoundAcpiTable = FALSE;
    for (Index = 0; Index < SystemTable->NumberOfTableEntries; Index++) {
      if (MyCompareGuid (
          &gEfiAcpiTableGuid,
          &(SystemTable->ConfigurationTable[Index].VendorGuid)
          ))
      {
        RsdpPtr = (UINT8 *)SystemTable->ConfigurationTable[Index].VendorTable;
        FoundAcpiTable = TRUE;
        break;
      }
    }

    if (FoundAcpiTable) {
      RsdpRevision = *(RsdpPtr + 15);

      if (RsdpRevision < 2) {
        Print(L"ERROR: RSDP revision is less than 2.\n");
        return EFI_UNSUPPORTED;
      }

      PrintTable("RSDP", RsdpPtr, 8, 4, 20, 6, 9, 1, 32); // RSDP Table
      UINT8 *RsdtPtr = (UINT8 *)(UINTN)(*(UINT32 *)(RsdpPtr + 16));
      PrintTable("RSDT", RsdtPtr, 4, 4, 4, 6, 10, 1, 9); // RSDT Table
      UINT8 *XsdtPtr = (UINT8 *)(UINTN)(*(UINT64 *)(RsdpPtr + 24));
      PrintTable("XSDT", XsdtPtr, 4, 4, 4, 6, 10, 1, 9); // XSDT Table
      for (Index = 0; Index < (*(UINT32 *)(XsdtPtr + 4) - 36) / 8; Index++) {
        UINT8 *UnKnownPtr = (UINT8 *)(UINTN)(*(UINT64 *)(XsdtPtr + 36 + Index * 8));
        ChangeTable(UnKnownPtr);
        PrintTable(NULL, UnKnownPtr, 4, 4, 4, 6, 10, 1, 9); // Unknown Table
      }
      UINT8 *FadtPtr = (UINT8 *)(UINTN)(*(UINT64 *)(XsdtPtr + 36));
      // UINT8 *FacsPtr = (UINT8 *)(UINTN)(*(UINT32 *)(FadtPtr + 36));
      UINT8 *DsdtPtr = (UINT8 *)(UINTN)(*(UINT32 *)(FadtPtr + 40));
      // PrintTable("FACS", FacsPtr, 4, 4, 4, 6, 10, 1, 9); // FACS Table
      PrintTable("DSDT", DsdtPtr, 4, 4, 4, 6, 10, 1, 9); // DSDT Table
    }
    else
    {
      Print(L"ERROR: Failed to find ACPI Table Guid in System Configuration Table.\n");
      return EFI_NOT_FOUND;
    }

    if (EFI_ERROR(Status)) {
      
    }
  }

  return ShellStatus;
}