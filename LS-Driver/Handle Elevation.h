//#pragma once
//
//#include <ntifs.h>
//#include <minwindef.h>
//
//
//
//// δ�����ľ������Ŀ�ṹ
//typedef struct _HANDLE_TABLE_ENTRY {
//	union {
//		ULONG_PTR VolatileLowValue;
//		ULONG_PTR LowValue;
//		struct _HANDLE_TABLE_ENTRY_INFO* InfoTable;
//		struct {
//			ULONG64 Unlocked : 1;
//			ULONG64 RefCnt : 16;
//			ULONG64 Attributes : 3;
//			ULONG64 ObjectPointerBits : 44;
//		};
//	};
//	union {
//		ULONG_PTR HighValue;
//		struct _HANDLE_TABLE_ENTRY* NextFreeHandleEntry;
//		ULONG_PTR LeafHandleValue;
//		struct {
//			ULONG32 GrantedAccessBits : 25;
//			ULONG32 NoRightsUpgrade : 1;
//			ULONG32 Spare : 6;
//		};
//	};
//	ULONG TypeInfo;
//} HANDLE_TABLE_ENTRY, * PHANDLE_TABLE_ENTRY;
//
//// δ�����ľ����ṹ
//typedef struct _HANDLE_TABLE {
//	ULONG_PTR TableCode;
//	struct _EPROCESS* QuotaProcess;
//	HANDLE UniqueProcessId;
//	void* HandleLock;
//	struct _LIST_ENTRY HandleTableList;
//	EX_PUSH_LOCK HandleContentionEvent;
//	struct _HANDLE_TRACE_DEBUG_INFO* DebugInfo;
//	int ExtraInfoPages;
//	ULONG Flags;
//	ULONG FirstFreeHandle;
//	struct _HANDLE_TABLE_ENTRY* LastFreeHandleEntry;
//	ULONG HandleCount;
//	ULONG NextHandleNeedingPool;
//} HANDLE_TABLE, * PHANDLE_TABLE;
//
//// �Զ���Hook֪ͨ�������ṹ
//typedef struct _HOOK_NOTIFY_BUFFER {
//	ULONG Enable;
//	PVOID HookPoint;
//	UCHAR NewBytes[13];
//	UCHAR OldBytes[13];
//	PVOID NotifyHandle;
//	LARGE_INTEGER Cookie;
//} HOOK_NOTIFY_BUFFER, * PHOOK_NOTIFY_BUFFER;
//
//// ��̬���ݽṹ�����ڴ洢ϵͳ��Ϣ��ƫ����
//typedef struct _DYNDATA {
//	ULONG UserVerify;
//	ULONG WinVersion;
//	ULONG BuildNumber;
//	ULONG VadRoot;
//	ULONG PrcessId;
//	ULONG Protection;
//	ULONG PspCidTable;
//	ULONG ProcessLinks;
//	ULONG PrcessIdOffset;
//	ULONG ParentPrcessIdOffset;
//	PBYTE KernelBase;
//	PBYTE DriverBase;
//	PBYTE ModuleList;
//	PBYTE Decryption;
//	PBYTE PageTables[4];
//	PBYTE NtCreateThreadEx;
//	PBYTE NtProtectVirtualMemory;
//} DYNDATA, * PDYNDATA;
//
//// �Զ���Windows�汾ö��
//typedef enum _WIN_VERSION {
//	WINVER_7 = 0x0611,
//	WINVER_8 = 0x0620,
//	WINVER_8X = 0x0630,
//	WINVER_1X = 0x0A00,
//} WIN_VERSION;
//
//// ȫ�ֱ����洢ƫ����
//// ��ʵ����Ŀ�У����ֵӦ���Ƕ�̬��ȡ�ģ�����Ϊ����ʾֱ��Ӳ����
//// ���磬���� Windows 10 x64 19041, ���ƫ���� 0x87A
//DYNDATA g_DynData = { 0x87A };
//
//// _PS_PROTECTION ��һ��δ�����Ľṹ�����䶨������֪��
//typedef struct _PS_PROTECTION {
//	UCHAR Type : 3;
//	UCHAR Audit : 1;
//	UCHAR Signer : 4;
//} PS_PROTECTION, * PPS_PROTECTION;
//
//
//// ����Windows 7���ö�ٵĻص�����ָ��
//typedef BOOLEAN(*Q_EX_ENUMERATE_HANDLE_ROUTINE_WIN7)(PHANDLE_TABLE_ENTRY, HANDLE, HANDLE);
//
//// ����Windows 8�����߰汾���ö�ٵĻص�����ָ��
//typedef BOOLEAN(*Q_EX_ENUMERATE_HANDLE_ROUTINE_WINX)(PHANDLE_TABLE, PHANDLE_TABLE_ENTRY, HANDLE, HANDLE);
//
//// ȫ�ֱ�������
//PHANDLE_TABLE_ENTRY SafeHandleTable = NULL;
//PHOOK_NOTIFY_BUFFER pSafeThreadHookBuffer;
//PDYNDATA DynamicData = NULL;
//
//// ���ֽ����������������
//inline __forceinline static LPBYTE XorByte(LPBYTE Dst, LPBYTE Src, SIZE_T Size) {
//	for (ULONG i = NULL; i < Size; i++) {
//		Dst[i] = (BOOLEAN)(Src[i] != 0x00 && Src[i] != 0xFF) ? Src[i] ^ 0xFF : Src[i];
//	}
//	return Dst;
//}
//
//// ��̬��ȡ������ Windows 7 �� ExEnumHandleTable ������
//inline __forceinline static BOOLEAN ExEnumHandleTable_Win7(PHANDLE_TABLE pHandleTable, Q_EX_ENUMERATE_HANDLE_ROUTINE_WIN7 EnumHandleProcedure, HANDLE pEnumParameter, PHANDLE Handle) {
//	typedef BOOLEAN(*fn_ExEnumHandleTable_Win7)(PHANDLE_TABLE, Q_EX_ENUMERATE_HANDLE_ROUTINE_WIN7, HANDLE, PHANDLE);
//	static fn_ExEnumHandleTable_Win7 _ExEnumHandleTable_Win7 = NULL;
//	BOOLEAN Status = FALSE;
//
//	if (_ExEnumHandleTable_Win7 == NULL) {
//		BYTE ShellCode[] = {
//			0xBA, 0x00, 0x87, 0x00, 0xBA, 0x00, 0x91, 0x00, 0x8A, 0x00, 0x92, 0x00, 0xB7, 0x00, 0x9E, 0x00, 0x91, 0x00, 0x9B, 0x00, 0x93, 0x00, 0x9A, 0x00, 0xAB, 0x00, 0x9E, 0x00, 0x9D, 0x00, 0x93, 0x00, 0x9A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
//		};
//		UNICODE_STRING RoutineName = { 0 };
//		RtlInitUnicodeString(&RoutineName, (PCWSTR)(XorByte(ShellCode, ShellCode, sizeof(ShellCode))));
//		_ExEnumHandleTable_Win7 = (fn_ExEnumHandleTable_Win7)(MmGetSystemRoutineAddress(&RoutineName));
//	}
//
//	if (_ExEnumHandleTable_Win7 != NULL) {
//		Status = _ExEnumHandleTable_Win7(pHandleTable, EnumHandleProcedure, pEnumParameter, Handle);
//	}
//	return Status;
//}
//
//// ��̬��ȡ������ Windows 8+ �� ExEnumHandleTable ������
//inline __forceinline static BOOLEAN ExEnumHandleTable_WinX(PHANDLE_TABLE pHandleTable, Q_EX_ENUMERATE_HANDLE_ROUTINE_WINX EnumHandleProcedure, HANDLE pEnumParameter, PHANDLE Handle) {
//	typedef BOOLEAN(*fn_ExEnumHandleTable_WinX)(PHANDLE_TABLE, Q_EX_ENUMERATE_HANDLE_ROUTINE_WINX, HANDLE, PHANDLE);
//	static fn_ExEnumHandleTable_WinX _ExEnumHandleTable_WinX = NULL;
//	BOOLEAN Status = FALSE;
//
//	if (_ExEnumHandleTable_WinX == NULL) {
//		BYTE ShellCode[] = {
//			0xBA, 0x00, 0x87, 0x00, 0xBA, 0x00, 0x91, 0x00, 0x8A, 0x00, 0x92, 0x00, 0xB7, 0x00, 0x9E, 0x00, 0x91, 0x00, 0x9B, 0x00, 0x93, 0x00, 0x9A, 0x00, 0xAB, 0x00, 0x9E, 0x00, 0x9D, 0x00, 0x93, 0x00, 0x9A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
//		};
//		UNICODE_STRING RoutineName = { 0 };
//		RtlInitUnicodeString(&RoutineName, (PCWSTR)(XorByte(ShellCode, ShellCode, sizeof(ShellCode))));
//		_ExEnumHandleTable_WinX = (fn_ExEnumHandleTable_WinX)(MmGetSystemRoutineAddress(&RoutineName));
//	}
//
//	if (_ExEnumHandleTable_WinX != NULL) {
//		Status = _ExEnumHandleTable_WinX(pHandleTable, EnumHandleProcedure, pEnumParameter, Handle);
//	}
//	return Status;
//}
//
//// ��̬��ȡ������ ExfUnblockPushLock �����Խ���������
//inline __forceinline static BOOLEAN ExfUnblockPushLock(PEX_PUSH_LOCK PushLock, LPVOID pWaitBlock) {
//	typedef VOID(*fn_ExfUnblockPushLock)(PEX_PUSH_LOCK, LPVOID);
//	static fn_ExfUnblockPushLock _ExfUnblockPushLock = NULL;
//
//	if (_ExfUnblockPushLock == NULL) {
//		BYTE ShellCode[] = {
//			0xBA, 0x00, 0x87, 0x00, 0x99, 0x00, 0xAA, 0x00, 0x91, 0x00, 0x9D, 0x00, 0x93, 0x00, 0x90, 0x00, 0x9C, 0x00, 0x94, 0x00, 0xAF, 0x00, 0x8A, 0x00, 0x8C, 0x00, 0x97, 0x00, 0xB3, 0x00, 0x90, 0x00, 0x9C, 0x00, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
//		};
//		UNICODE_STRING RoutineName = { NULL };
//		RtlInitUnicodeString(&RoutineName, (PCWSTR)(XorByte(ShellCode, ShellCode, sizeof(ShellCode))));
//		_ExfUnblockPushLock = (fn_ExfUnblockPushLock)(MmGetSystemRoutineAddress(&RoutineName));
//	}
//
//	if (_ExfUnblockPushLock != NULL) {
//		_ExfUnblockPushLock(PushLock, pWaitBlock);
//	}
//	return _ExfUnblockPushLock != NULL;
//}
//
//// ���� Windows 7 �ľ��ö�ٻص��������������Ȩ�ޡ�
//inline __forceinline static BOOLEAN HandleCallback_Win7(PHANDLE_TABLE_ENTRY pHandleTableEntry, HANDLE Handle, HANDLE MyHandle) {
//	BOOLEAN Result = FALSE;
//	if (pHandleTableEntry != NULL && Handle == MyHandle) {
//		pHandleTableEntry->GrantedAccessBits = PROCESS_ALL_ACCESS;
//		Result = TRUE;
//	}
//	return Result;
//}
//
//// ���� Windows 8+ �ľ��ö�ٻص��������������Ȩ�޲���������
//inline __forceinline static BOOLEAN HandleCallback_WinX(PHANDLE_TABLE pHandleTable, PHANDLE_TABLE_ENTRY pHandleTableEntry, HANDLE Handle, HANDLE MyHandle) {
//	BOOLEAN Result = FALSE;
//	if (pHandleTableEntry != NULL && Handle == MyHandle) {
//		pHandleTableEntry->GrantedAccessBits = PROCESS_ALL_ACCESS;
//		Result = TRUE;
//	}
//
//	if (pHandleTable != NULL) {
//		if (pHandleTable->HandleContentionEvent) {
//			ExfUnblockPushLock(&pHandleTable->HandleContentionEvent, NULL);
//		}
//	}
//
//	if (pHandleTableEntry != NULL) {
//		_InterlockedExchangeAdd8((PCHAR)&pHandleTableEntry->VolatileLowValue, 1);
//	}
//	return Result;
//}
//
//// ���ݲ���ϵͳ�汾������ָ���������ض�����ķ���Ȩ�ޡ�
//inline __forceinline static NTSTATUS HandleGrantAccess(PEPROCESS pProcess, HANDLE MyHandle) {
//	NTSTATUS Status = STATUS_UNSUCCESSFUL;
//	if (DynamicData->WinVersion <= WINVER_7) {
//		Status = ExEnumHandleTable_Win7(*(PHANDLE_TABLE*)((PBYTE)pProcess + DynamicData->PspCidTable), &HandleCallback_Win7, MyHandle, NULL) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
//	}
//
//	if (DynamicData->WinVersion >= WINVER_8) {
//		Status = ExEnumHandleTable_WinX(*(PHANDLE_TABLE*)((PBYTE)pProcess + DynamicData->PspCidTable), &HandleCallback_WinX, MyHandle, NULL) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
//	}
//	return Status;
//}
//
//
//
//
////���ⲿ����(�������ػ��ߴ��豸ʱ)���г�ʼ��
//inline __forceinline static NTSTATUS InitializeDynamicData()
//{
//	// ����Ѿ���ʼ������ֱ�ӷ��سɹ�
//	if (DynamicData != NULL)
//	{
//		return STATUS_SUCCESS;
//	}
//
//	// [�޸�] ʹ�� ExAllocatePoolWithTag ���ں�ģʽ�·����ڴ�
//	DynamicData = (PDYNDATA)ExAllocatePoolWithTag(NonPagedPool, sizeof(DYNDATA), 'dynd');
//	if (DynamicData == NULL)
//	{
//		return STATUS_INSUFFICIENT_RESOURCES;
//	}
//
//	// ��������ڴ�����
//	RtlZeroMemory(DynamicData, sizeof(DYNDATA));
//
//	// [�޸�] ������ִ�д�������˺���
//	RTL_OSVERSIONINFOEXW WinVersion = { sizeof(RTL_OSVERSIONINFOEXW) };
//	RtlGetVersion((PRTL_OSVERSIONINFOW)&WinVersion);
//
//	DynamicData->WinVersion = WinVersion.dwMajorVersion << 8 | WinVersion.dwMinorVersion << 4 | WinVersion.wServicePackMajor;
//	DynamicData->BuildNumber = WinVersion.dwBuildNumber;
//
//	DynamicData->VadRoot = DynamicData->WinVersion <= WINVER_7 ? 0x448 : (DynamicData->WinVersion <= WINVER_8 ? 0x590 : (DynamicData->WinVersion <= WINVER_8X ? 0x5D8 : (DynamicData->BuildNumber <= 10240 ? 0x608 : (DynamicData->BuildNumber <= 10586 ? 0x610 : (DynamicData->BuildNumber <= 14393 ? 0x620 : (DynamicData->BuildNumber <= 17763 ? 0x628 : (DynamicData->BuildNumber <= 18850 ? 0x658 : (DynamicData->BuildNumber <= 18865 ? 0x698 : 0x7D8))))))));
//	DynamicData->PrcessId = DynamicData->WinVersion <= WINVER_7 ? 0x180 : (DynamicData->WinVersion <= WINVER_8X ? 0x2E0 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 14393) ? 0x2E8 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 17763) ? 0x2E0 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 18363) ? 0x2E8 : 0x440))));
//	DynamicData->Protection = DynamicData->WinVersion <= WINVER_7 ? 0x43C : (DynamicData->WinVersion == WINVER_8 ? 0x648 : (DynamicData->WinVersion == WINVER_8X ? 0x67A : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 10586) ? 0x6B2 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 17763) ? 0x6CA : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 18363) ? 0x6FA : 0x87A)))));
//	DynamicData->PspCidTable = DynamicData->WinVersion <= WINVER_7 ? 0x200 : (DynamicData->WinVersion <= WINVER_8X ? 0x408 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 18363) ? 0x418 : 0x570));
//	DynamicData->ProcessLinks = DynamicData->WinVersion <= WINVER_7 ? 0x188 : (DynamicData->WinVersion <= WINVER_8X ? 0x2E8 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 14393) ? 0x2F0 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 17763) ? 0x2E8 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 18363) ? 0x2F0 : 0x448))));
//	DynamicData->PrcessIdOffset = DynamicData->WinVersion <= WINVER_7 ? 0x180 : (DynamicData->WinVersion <= WINVER_8X ? 0x2E0 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 14393) ? 0x2E8 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 17763) ? 0x2E0 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 18363) ? 0x2E8 : 0x440))));
//	DynamicData->ParentPrcessIdOffset = DynamicData->WinVersion <= WINVER_7 ? 0x290 : (DynamicData->WinVersion <= WINVER_8X ? 0x3E0 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 14393) ? 0x3E0 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 17763) ? 0x3E0 : ((DynamicData->WinVersion == WINVER_1X && DynamicData->BuildNumber <= 18363) ? 0x3E8 : 0x540))));
//
//	return STATUS_SUCCESS;
//}
//
