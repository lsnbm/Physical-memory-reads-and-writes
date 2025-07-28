#pragma once
#include<ntifs.h>
#include "PEB.h"
#include <ntstrsafe.h> // ���ڰ�ȫ�ַ�������

#define WINDOWS_1803 17134
#define WINDOWS_1809 17763
#define WINDOWS_1903 18362
#define WINDOWS_1909 18363
#define WINDOWS_2004 19041
#define WINDOWS_20H2 19569
#define WINDOWS_21H2 20180
#define WINDOWS_22H2 22621
#define PAGE_OFFSET_SIZE 12



//from https://www.unknowncheats.me/forum/anti-cheat-bypass/444289-read-process-physical-memory-attach.html
//���ݵ�ǰ����ϵͳ�İ汾�ŷ���һ���ض���ƫ��ֵ
__forceinline  UINT32 getoffsets()
{
	RTL_OSVERSIONINFOW ver;
	RtlGetVersion(&ver);

	switch (ver.dwBuildNumber)
	{
	case WINDOWS_1803:
	case WINDOWS_1809:
		return 0x0278;

	case WINDOWS_1903:
	case WINDOWS_1909:
		return 0x0280;

		// WINDOWS_2004, 20H2, 21H2, 22H2 �Լ����и��µİ汾
	default:
		return 0x0388;
	}
}

//��ȡָ�����̵�ҳĿ¼����ַ
 __forceinline  ULONG_PTR getprocessdirbase(PEPROCESS targetprocess)
{
	if (!targetprocess)return 0;

	ULONG_PTR dirbase = *(PULONG_PTR)((PUCHAR)targetprocess + 0x28);

	if (!dirbase)
	{
		static const UINT32 fallback_offset = getoffsets();
		dirbase = *(PULONG_PTR)((PUCHAR)targetprocess + fallback_offset);
	}

	return dirbase;
}


//�������ַ�������ݵ�ָ���Ļ�����
 __forceinline  NTSTATUS readphysaddress(PVOID address, PVOID buffer, SIZE_T size, SIZE_T* read)
{
	MM_COPY_ADDRESS addr; addr.PhysicalAddress.QuadPart = (LONGLONG)address;
	return MmCopyMemory(buffer, addr, size, MM_COPY_MEMORY_PHYSICAL, read);
}

//�ӻ��������Ƶ�ַ�������ַ
 __forceinline  NTSTATUS writephysaddress(PVOID address, PVOID buffer, SIZE_T size, SIZE_T* written)
{
	PHYSICAL_ADDRESS addr; addr.QuadPart = (LONGLONG)address;

	PVOID mapped_mem = MmMapIoSpaceEx(addr, size, PAGE_READWRITE);

	if (!mapped_mem)return STATUS_UNSUCCESSFUL;

	memcpy(mapped_mem, buffer, size);

	*written = size;
	MmUnmapIoSpace(mapped_mem, size);
	return STATUS_SUCCESS;
}

//�������������ַת��Ϊ�����ַ��ͨ������ҳ���νṹ��ȷ�������ַ�����Ĵ��벻����Ai
 __forceinline  ULONG64 translateaddress(ULONG64 processdirbase, ULONG64 address)
{
	//constexpr����ʱ����
	 static constexpr UINT64 mask = (~0xfull << 8) & 0xfffffffffull;//�������������λ�ı�־λ��ֻ�ܴ������64g�ڴ棩
	processdirbase &= ~0xf;

	ULONG64  pageoffset = address & ~(~0ul << PAGE_OFFSET_SIZE);
	ULONG64  pte = ((address >> 12) & (0x1ffll));
	ULONG64  pt = ((address >> 21) & (0x1ffll));
	ULONG64  pd = ((address >> 30) & (0x1ffll));
	ULONG64  pdp = ((address >> 39) & (0x1ffll));

	SIZE_T readsize;
	ULONG64  pdpe;
	readphysaddress((void*)(processdirbase + 8 * pdp), &pdpe, sizeof(pdpe), &readsize);
	if (~pdpe & 1)
		return 0;

	ULONG64  pde;
	readphysaddress((void*)((pdpe & mask) + 8 * pd), &pde, sizeof(pde), &readsize);
	if (~pde & 1)
		return 0;

	if (pde & 0x80)
		return (pde & (~0ull << 42 >> 12)) + (address & ~(~0ull << 30));

	ULONG64  ptraddr;
	readphysaddress((void*)((pde & mask) + 8 * pt), &ptraddr, sizeof(ptraddr), &readsize);
	if (~ptraddr & 1)
		return 0;

	if (ptraddr & 0x80)
		return (ptraddr & mask) + (address & ~(~0ull << 21));

	address = 0;
	readphysaddress((void*)((ptraddr & mask) + 8 * pte), &address, sizeof(address), &readsize);
	address &= mask;

	if (!address)
		return 0;

	return address + pageoffset;
}


 __forceinline  NTSTATUS ReadPhysMemory(ULONG_PTR process_dirbase, PVOID address, PVOID buffer, SIZE_T size, SIZE_T* read) {
	SIZE_T curoffset = 0;
	while (size)
	{
		//�������ض��������ַ���ɴ���� address ���ϵ�ǰƫ���� curoffset��ת��Ϊ�����ַ
		ULONG64 addr = translateaddress(process_dirbase, (ULONG64)address + curoffset);
		if (!addr) return STATUS_UNSUCCESSFUL;


		//���㱾��Ҫ��ȡ���ֽ���
		ULONG64 readsize = min(PAGE_SIZE - (addr & 0xFFF), size);
		SIZE_T readreturn;

		if (!readphysaddress((void*)addr, (PVOID)((ULONG64)buffer + curoffset), readsize, &readreturn) || readreturn == 0)break;

		size -= readreturn;     //����ʣ��Ҫ��ȡ���ֽ���
		curoffset += readreturn;//����ƫ������ָ����һ��Ҫ��ȡ��λ��

	}

	//*read = curoffset;
	return STATUS_SUCCESS;
}

 __forceinline  NTSTATUS WritePhysMemory(ULONG_PTR process_dirbase, PVOID address, PVOID buffer, SIZE_T size, SIZE_T* write)
{
	SIZE_T curoffset = 0;
	while (size)
	{
		auto addr = translateaddress(process_dirbase, (ULONG64)address + curoffset);
		if (!addr) return STATUS_UNSUCCESSFUL;

		ULONG64 writesize = min(PAGE_SIZE - (addr & 0xFFF), size);
		SIZE_T written;

		if (!writephysaddress((void*)addr, (PVOID)((ULONG64)buffer + curoffset), writesize, &written) || written == 0)break;


		size -= written;
		curoffset += written;

	}

	//*write = curoffset;
	return STATUS_SUCCESS;
}

 __forceinline  ULONG64 GetModuleBase(PEPROCESS TarGetProcess, const char* ModuleName, ULONG64* get_size)
{
	NTSTATUS status;

	// 1. ANSI �� ANSI_STRING
	ANSI_STRING ansiName;
	RtlInitAnsiString(&ansiName, ModuleName);

	// 2. ANSI_STRING �� UNICODE_STRING���ں˳ط��䣩
	UNICODE_STRING moduleName;
	RtlAnsiStringToUnicodeString(&moduleName, &ansiName, TRUE);


	PPEB pPeb = (PPEB)PsGetProcessPeb(TarGetProcess); // get Process PEB, function is unexported and undoc

	if (!pPeb)return 0;




	KAPC_STATE state;
	KeStackAttachProcess(TarGetProcess, &state);

	PPEB_LDR_DATA pLdr = (PPEB_LDR_DATA)pPeb->Ldr;

	if (!pLdr) {
		KeUnstackDetachProcess(&state);
		return 0; // failed
	}

	UNICODE_STRING name;

	// loop the linked list
	for (PLIST_ENTRY list = (PLIST_ENTRY)pLdr->InLoadOrderModuleList.Flink;
		list != &pLdr->InLoadOrderModuleList; list = (PLIST_ENTRY)list->Flink)
	{
		PLDR_DATA_TABLE_ENTRY pEntry = CONTAINING_RECORD(list, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

		DbgPrint("Module Name: %wZ\n", pEntry->BaseDllName);
		DbgPrint("Module Base: %p\n", pEntry->DllBase);
		DbgPrint("Module Size: %d\n", pEntry->SizeOfImage);

		if (RtlCompareUnicodeString(&pEntry->BaseDllName, &moduleName, TRUE) == 0) {
			ULONG64 baseAddr = (ULONG64)pEntry->DllBase;
			ULONG64 moduleSize = (ULONG64)pEntry->SizeOfImage; // get the size of the module
			KeUnstackDetachProcess(&state);

			*get_size = moduleSize; // return the size of the module if get_size is TRUE

			return baseAddr;
		}
	}

	KeUnstackDetachProcess(&state);

	return STATUS_SUCCESS;
}

 __forceinline  NTSTATUS StopProcess(ULONG pid)
{
	NTSTATUS status;
	PEPROCESS process = NULL;

	// ����PID����EPROCESS�ṹ
	status = PsLookupProcessByProcessId((HANDLE)pid, &process);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to find process with PID: %u, Status: %X\n", pid, status);
		return status;
	}

	// ����������Ѿ�����һ���Խ��̶�������ã�process����
	// �����������ü����Ѿ������ˡ�

	HANDLE processHandle = NULL;
	// ��ȡ���̵��ں˾�����Ա�ʹ��ZwTerminateProcess
	status = ObOpenObjectByPointer(
		process,
		OBJ_KERNEL_HANDLE, // ָ�����������ں˾��
		NULL,
		STANDARD_RIGHTS_ALL, // ���з���Ȩ��
		*PsProcessType,
		KernelMode,
		&processHandle
	);

	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to get process handle. Status: %X\n", status);
		ObDereferenceObject(process); // ����ʱ����Ҫ�����ͷŶ�process������
		return status;
	}

	// ʹ�û�ȡ���ľ����ֹ����
	status = ZwTerminateProcess(processHandle, 0); // 0 ��ʾ�ɹ����˳���

	// ������ֹ�Ƿ�ɹ������Ƕ���Ҫ�رվ�����ͷŶ�process���������
	ObCloseHandle(processHandle, KernelMode);
	ObDereferenceObject(process);

	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to terminate process. Status: %X\n", status);
	}
	else {
		DbgPrint("Process terminated successfully.\n");
	}

	return status;
}

 __forceinline   NTSTATUS DumpAndFixProcessByPidOptimized(IN HANDLE ProcessId) {
	NTSTATUS status = STATUS_SUCCESS;
	PEPROCESS pEprocess = NULL;
	PVOID imageBuffer = NULL;
	HANDLE fileHandle = NULL;
	KAPC_STATE apcState;
	BOOLEAN isAttached = FALSE;

	// 1. ����ID���ҽ��̶���
	status = PsLookupProcessByProcessId(ProcessId, &pEprocess);
	if (!NT_SUCCESS(status)) {
		// DbgPrint("[-] Process with PID %p not found. Status: 0x%X\n", ProcessId, status);
		return status;
	}

	__try {
		// 2. ���ӵ�Ŀ����̵ĵ�ַ�ռ� (��������ֻ����һ��)
		KeStackAttachProcess(pEprocess, &apcState);
		isAttached = TRUE;

		// 3. ���ƽ�����ģ�鵽�ں˻�����
		PVOID imageBase = NULL;
		SIZE_T imageSize = 0;
		__try {
			PPEB pPeb = PsGetProcessPeb(pEprocess);
			if (!pPeb || !pPeb->ImageBaseAddress) {
				status = STATUS_NOT_FOUND;
				__leave;
			}
			imageBase = pPeb->ImageBaseAddress;

			PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)imageBase;
			PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((PUCHAR)imageBase + pDosHeader->e_lfanew);
			imageSize = pNtHeaders->OptionalHeader.SizeOfImage;

			imageBuffer = ExAllocatePoolWithTag(PagedPool, imageSize, 'DUMP');
			if (!imageBuffer) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				__leave;
			}
			RtlCopyMemory(imageBuffer, imageBase, imageSize);

			// �����ǵĻ����������У���ImageBase����Ϊ��ʵ�ʼ��ص�ַ
			PIMAGE_NT_HEADERS pNtHeadersInDump = (PIMAGE_NT_HEADERS)((PUCHAR)imageBuffer + pDosHeader->e_lfanew);
			pNtHeadersInDump->OptionalHeader.ImageBase = (ULONGLONG)imageBase;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			status = GetExceptionCode();
			// DbgPrint("[-] Exception while copying process image. Status: 0x%X\n", status);
			__leave; // �뿪��__try��
		}

		// 4. �޸������ (�ڸ���״̬�½���)
		if (NT_SUCCESS(status)) {
			PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)imageBuffer;
			PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((PUCHAR)imageBuffer + pDosHeader->e_lfanew);
			IMAGE_DATA_DIRECTORY importDirData = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

			if (importDirData.Size > 0) {
				PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((PUCHAR)imageBuffer + importDirData.VirtualAddress);

				while (pImportDesc->Name) {
					CHAR* dllNameAnsi = (CHAR*)((PUCHAR)imageBuffer + pImportDesc->Name);
					UNICODE_STRING dllNameUnicode;
					ANSI_STRING dllNameAnsiStr;
					RtlInitAnsiString(&dllNameAnsiStr, dllNameAnsi);
					// ��DLL����ת��ΪUnicode�Խ��бȽ�
					if (!NT_SUCCESS(RtlAnsiStringToUnicodeString(&dllNameUnicode, &dllNameAnsiStr, TRUE))) {
						pImportDesc++;
						continue;
					}

					// 5. ��Ч����ģ���ַ
					PVOID moduleBase = NULL;
					PPEB pPeb = PsGetProcessPeb(pEprocess); // �ٴλ�ȡPEBָ��
					if (pPeb && pPeb->Ldr) {
						for (PLIST_ENTRY pListEntry = pPeb->Ldr->InLoadOrderModuleList.Flink;
							pListEntry != &pPeb->Ldr->InLoadOrderModuleList;
							pListEntry = pListEntry->Flink)
						{
							PLDR_DATA_TABLE_ENTRY pLdrEntry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
							if (RtlCompareUnicodeString(&pLdrEntry->BaseDllName, &dllNameUnicode, TRUE) == 0) {
								moduleBase = pLdrEntry->DllBase;
								break;
							}
						}
					}
					RtlFreeUnicodeString(&dllNameUnicode); // �ͷ�ת��ʱ������ڴ�

					if (moduleBase) {
						PIMAGE_THUNK_DATA pOriginalFirstThunk = (PIMAGE_THUNK_DATA)((PUCHAR)imageBuffer + pImportDesc->OriginalFirstThunk);
						PIMAGE_THUNK_DATA pFirstThunk = (PIMAGE_THUNK_DATA)((PUCHAR)imageBuffer + pImportDesc->FirstThunk);

						while (pOriginalFirstThunk->u1.AddressOfData) {
							PVOID functionAddress = NULL;
							if (IMAGE_SNAP_BY_ORDINAL(pOriginalFirstThunk->u1.Ordinal)) {
								// �ݲ�֧�ְ���ŵ�����޸������������
							}
							else {
								PIMAGE_IMPORT_BY_NAME pImportByName = (PIMAGE_IMPORT_BY_NAME)((PUCHAR)imageBuffer + pOriginalFirstThunk->u1.AddressOfData);
								// 6. ʹ���ں�API��ȫ���Һ�����ַ
								functionAddress = RtlFindExportedRoutineByName(moduleBase, (PCSTR)pImportByName->Name);
							}

							// 7. ���޸��ĵ�ַд��IAT
							pFirstThunk->u1.Function = (ULONGLONG)functionAddress;

							pOriginalFirstThunk++;
							pFirstThunk++;
						}
					}
					pImportDesc++;
				}
			}
		}

		// 8. д���ļ� (�ڷ�������)
		KeUnstackDetachProcess(&apcState);
		isAttached = FALSE;

		if (NT_SUCCESS(status)) {
			UNICODE_STRING uniFilePath;
			OBJECT_ATTRIBUTES objAttributes;
			IO_STATUS_BLOCK ioStatusBlock;
			WCHAR filePathBuffer[256];

			RtlStringCchPrintfW(filePathBuffer, RTL_NUMBER_OF(filePathBuffer), L"\\??\\C:\\Temp\\%p_fixed_dump.dmp", ProcessId);
			RtlInitUnicodeString(&uniFilePath, filePathBuffer);
			InitializeObjectAttributes(&objAttributes, &uniFilePath, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

			status = ZwCreateFile(&fileHandle, FILE_GENERIC_WRITE, &objAttributes, &ioStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
			if (NT_SUCCESS(status)) {
				status = ZwWriteFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock, imageBuffer, (ULONG)imageSize, NULL, NULL);
			}
		}
	}
	__finally {
		// 9. ȷ��������Դ�����ͷ�
		if (isAttached) {
			KeUnstackDetachProcess(&apcState);
		}
		if (fileHandle) {
			ZwClose(fileHandle);
		}
		if (imageBuffer) {
			ExFreePoolWithTag(imageBuffer, 'DUMP');
		}
		if (pEprocess) {
			ObDereferenceObject(pEprocess);
		}
	}
	return status;
}
