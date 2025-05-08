#pragma once
#include <ntifs.h>
#include <ntddk.h>
#include <ntstrsafe.h> 
#include "wdm.h"
#define to_lower_i(Char) ((Char >= 'A' && Char <= 'Z') ? (Char + 32) : Char)
#define to_lower_c(Char) ((Char >= (char*)'A' && Char <= (char*)'Z') ? (Char + 32) : Char)
#ifndef DBG
#define LogPrint(fmt, ...)
#else
#define LogPrint(fmt, ...) DbgPrintEx(0, 0, fmt, __VA_ARGS__)
#endif

extern "C" NTSTATUS ZwQuerySystemInformation(ULONG InfoClass, PVOID Buffer, ULONG Length, PULONG ReturnLenght
);

extern "C" NTSTATUS NTAPI ExRaiseHardError(
    NTSTATUS ErrorStatus, ULONG NumberOfParameters,
    ULONG UnicodeStringParameterMask, PULONG_PTR Parameters,
    ULONG ValidResponseOptions, PULONG Response);

ULONG KeMessageBox(PCWSTR title, PCWSTR text, ULONG_PTR type)
{
    UNICODE_STRING uTitle = { 0 };
    UNICODE_STRING uText = { 0 };

    RtlInitUnicodeString(&uTitle, title);
    RtlInitUnicodeString(&uText, text);

    ULONG_PTR args[] = { (ULONG_PTR)&uText, (ULONG_PTR)&uTitle, type };
    ULONG response = 0;

    ExRaiseHardError(STATUS_SERVICE_NOTIFICATION, 3, 3, args, 2, &response);
    return response;
}

typedef enum _SYSTEM_INFORMATION_CLASS
{

    SystemBasicInformation,
    SystemProcessorInformation,
    SystemPerfmormanceInformation,
    SystemTimeOfDayInformation,
    SystemPathInformation,
    SystemProcessInformation,
    SystemCallCountInformation,
    SystemDeviceInformation,
    SystemProcessorPerformanceInformation,
    SystemFlagsInformation,
    SystemCallTimeInformation,
    SystemModuleInformation = 0x0B

} SYSTEM_INFORMATION_CLASS,
* PSYSTEM_INFORMATION_CLASS;


typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
    HANDLE Section;
    PVOID MappedBase;
    PVOID ImageBase;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoaderOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR FullPathName[256];

} RTL_PROCESS_MODULE_INFORMATION, * PRTL_PROCESS_MODULE_INFORMATION;

typedef struct _RTL_PROCESS_MODULES
{
    ULONG NumberOfModules;
    RTL_PROCESS_MODULE_INFORMATION Modules[1];

} RTL_PROCESS_MODULES, * PRTL_PROCESS_MODULES;