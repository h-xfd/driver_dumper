#include "definitions.h"

void convert_ansi_to_wide(const char* source, WCHAR* destination, size_t destination_size)
{
    size_t i = 0;
    while (source[i] != '\0' && i < destination_size - 1) {
        destination[i] = (WCHAR)source[i];
        i++;
    }
    destination[i] = L'\0';
}

NTSTATUS get_system_module(const char* module_name, PVOID* module_base, SIZE_T* size)
{
    ULONG bytes = 0;
    NTSTATUS status = ZwQuerySystemInformation(SystemModuleInformation, NULL, bytes, &bytes);

    if (!bytes)
        return NULL;

    PRTL_PROCESS_MODULES modules = (PRTL_PROCESS_MODULES)ExAllocatePoolWithTag(NonPagedPool, bytes, 0x68786664);

    if (!modules)
    {
        return NULL;
    }

    status = ZwQuerySystemInformation(SystemModuleInformation, modules, bytes, &bytes);

    if (!NT_SUCCESS(status))
        return NULL;

    for (ULONG i = 0; i < modules->NumberOfModules; i++)
    {
        const RTL_PROCESS_MODULE_INFORMATION& mod = modules->Modules[i];

        const char* full_path = (char*)mod.FullPathName;
        const char* file_name = strrchr(full_path, '\\');

        if (file_name != NULL) {
            file_name++;
        }
        else {
            file_name = full_path;
        }

        WCHAR file_name_wide[256];
        convert_ansi_to_wide(file_name, file_name_wide, sizeof(file_name_wide) / sizeof(file_name_wide[0]));

        WCHAR module_name_wide[256];
        convert_ansi_to_wide(module_name, module_name_wide, sizeof(module_name_wide) / sizeof(module_name_wide[0]));

        UNICODE_STRING unicode_file_name;
        RtlInitUnicodeString(&unicode_file_name, file_name_wide);

        UNICODE_STRING unicode_module_name;
        RtlInitUnicodeString(&unicode_module_name, module_name_wide);

        if (RtlCompareUnicodeString(&unicode_file_name, &unicode_module_name, TRUE) == 0)
        {
            *module_base = mod.ImageBase;
            *size = mod.ImageSize;

            return STATUS_SUCCESS;
        }
    }

    if (modules)
    {
        ExFreePoolWithTag(modules, 0x68786664);
    }

    if (module_base <= 0)
    {
        return 0;
    }

    return STATUS_UNSUCCESSFUL;
}

NTSTATUS dump(const char* module_name)
{
    LogPrint("[hxfd] dumping module: %s\n", module_name);

    PVOID module_base;
    SIZE_T size;

    if (!NT_SUCCESS(get_system_module(module_name, &module_base, &size))) {
        LogPrint("[hxfd] module not found: %s\n", module_name);
        return STATUS_NOT_FOUND;
    }

    if (!module_base || size == 0) {
        LogPrint("[hxfd] invalid parameters");
        return STATUS_INVALID_PARAMETER;
    }

    CHAR file_name_buffer[100];
    RtlStringCchPrintfA(file_name_buffer, sizeof(file_name_buffer), "\\??\\C:\\driver_dumper\\%s", module_name);

    LogPrint("[hxfd] module name: \\??\\C:\\driver_dumper\\%s\n", module_name);


    UNICODE_STRING unicode_file_name;
    WCHAR wide_buffer[100];
    ANSI_STRING ansi_name;
    RtlInitAnsiString(&ansi_name, file_name_buffer);
    RtlAnsiStringToUnicodeString(&unicode_file_name, &ansi_name, TRUE);

    OBJECT_ATTRIBUTES object_attributes;
    IO_STATUS_BLOCK io_status;
    HANDLE file_handle;

    InitializeObjectAttributes(
        &object_attributes,
        &unicode_file_name,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL
    );

    NTSTATUS status = ZwCreateFile(
        &file_handle,
        GENERIC_WRITE,
        &object_attributes,
        &io_status,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        0,
        FILE_OVERWRITE_IF,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0
    );

    RtlFreeUnicodeString(&unicode_file_name);

    if (!NT_SUCCESS(status)) {
        LogPrint("Failed to create file: 0x%X\n", status);
        return status;
    }

    status = ZwWriteFile(
        file_handle,
        NULL,
        NULL,
        NULL,
        &io_status,
        module_base,
        (ULONG)size,
        NULL,
        NULL
    );

    ZwClose(file_handle);

    if (!NT_SUCCESS(status)) {
        LogPrint("Failed to write file: 0x%X\n", status);
    }
    else {
        LogPrint("Saved module dump to file.\n");
    }

    return status;
}


extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status;
    IO_STATUS_BLOCK io_status_block;
    HANDLE handle;
    OBJECT_ATTRIBUTES object_attributes;

    const WCHAR* dir_path = L"\\??\\C:\\driver_dumper";
    UNICODE_STRING unicode_dir_path;
    RtlInitUnicodeString(&unicode_dir_path, dir_path);

    InitializeObjectAttributes(&object_attributes,
        &unicode_dir_path,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    status = ZwCreateFile(&handle,
        FILE_LIST_DIRECTORY | SYNCHRONIZE,
        &object_attributes,
        &io_status_block,
        NULL,
        FILE_ATTRIBUTE_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN_IF,
        FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0);

    if (!NT_SUCCESS(status)) {
        LogPrint("[hxfd] failed to create or open directory, code: %p\n", (void*)status);
        return STATUS_UNSUCCESSFUL;
    }

    ZwClose(handle);
    LogPrint("[hxfd] directory exists or was created\n");

    const WCHAR* file_path = L"\\??\\C:\\driver_dumper\\config.hxfd";
    UNICODE_STRING unicode_file_path;
    RtlInitUnicodeString(&unicode_file_path, file_path);

    InitializeObjectAttributes(&object_attributes,
        &unicode_file_path,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        NULL);

    status = ZwCreateFile(&handle,
        GENERIC_READ,
        &object_attributes,
        &io_status_block,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        FILE_OPEN,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0);

    // file exists
    if (NT_SUCCESS(status)) {
        LogPrint("[hxfd] config.hxfd exists\n");

        ULONG bytes_read = 0;
        CHAR buffer[1024]; 
        BOOLEAN file_empty = TRUE;

        while (TRUE) {
            status = ZwReadFile(
                handle,
                NULL,
                NULL,
                NULL,
                &io_status_block,
                buffer,
                sizeof(buffer) - 1,
                NULL,
                NULL
            );

            if (!NT_SUCCESS(status) && status != STATUS_END_OF_FILE) {
                LogPrint("[hxfd] failed to read file, error code: %p\n", (void*)status);
                ZwClose(handle);
                return STATUS_UNSUCCESSFUL;
            }

            if (io_status_block.Information > 0) {
                buffer[io_status_block.Information] = '\0';

                file_empty = FALSE;

                char* line = buffer;
                for (ULONG i = 0; i < io_status_block.Information; ++i) {
                    if (buffer[i] == '\n' || buffer[i] == '\r') {
                        buffer[i] = '\0';
                        LogPrint("[hxfd] Line: %s\n", line);
                        dump(line);
                        line = &buffer[i + 1];
                    }
                }

                if (line != buffer) {
                    LogPrint("[hxfd] Line: %s\n", line);
                    dump(line);
                }
            }

            if (io_status_block.Information == 0) {
                break;
            }
        }

        if (file_empty) {
            LogPrint("[hxfd] config.hxfd is empty\n");
            KeMessageBox(L"driver_dumper", L"C:\\driver_dumper\\config.hxfd is empty", 0x00000000L); // 0x00000000L = MB_OK
            ZwClose(handle);
            return STATUS_UNSUCCESSFUL;
        }

        ZwClose(handle);
        return STATUS_SUCCESS;
    }

    // if file doesn't exist
    if (status == STATUS_OBJECT_NAME_NOT_FOUND || status == STATUS_OBJECT_PATH_NOT_FOUND) {
        status = ZwCreateFile(&handle,
            GENERIC_WRITE,
            &object_attributes,
            &io_status_block,
            NULL,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            FILE_CREATE,
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
            NULL,
            0);

        if (!NT_SUCCESS(status)) {
            LogPrint("[hxfd] failed to create config.hxfd, error code: %p\n", (void*)status);
            return STATUS_UNSUCCESSFUL;
        }

        LogPrint("[hxfd] config.hxfd file was created\n");
        KeMessageBox(L"driver_dumper", L"C:\\driver_dumper\\config.hxfd was created, input the drivers you want to dump and map the driver again.", 0x00000000L); // 0x00000000L = MB_OK
        ZwClose(handle);
        return STATUS_SUCCESS;
    }

    LogPrint("[hxfd] creating config.hxfd failed, error code:: %p\n", (void*)status);
    return STATUS_UNSUCCESSFUL;
}
