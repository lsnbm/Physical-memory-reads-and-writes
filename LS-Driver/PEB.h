#pragma once
#include <ntifs.h>
#include <ntdef.h>  
#include <ntimage.h>  

typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
	HANDLE section;
	PVOID mapped_base;
	PVOID image_base;
	ULONG image_size;
	ULONG flags;
	USHORT load_order_index;
	USHORT init_order_index;
	USHORT load_count;
	USHORT offset_to_file_name;
	UCHAR  full_path_name[256];
} RTL_PROCESS_MODULE_INFORMATION, * PRTL_PROCESS_MODULE_INFORMATION;


typedef struct _RTL_PROCESS_MODULES
{
	ULONG number_of_modules;
	RTL_PROCESS_MODULE_INFORMATION modules[1];
} RTL_PROCESS_MODULES, * PRTL_PROCESS_MODULES;

typedef struct _PEB_LDR_DATA
{
	ULONG Length;
	UCHAR Initialized;
	PVOID SsHandle;
	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA, * PPEB_LDR_DATA;


typedef struct _RTL_USER_PROCESS_PARAMETERS {
	UCHAR Reserved1[16];
	PVOID Reserved2[10];
	UNICODE_STRING ImagePathName;
	UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS, * PRTL_USER_PROCESS_PARAMETERS;

typedef struct _PEB
{
	UCHAR InheritedAddressSpace;
	UCHAR ReadImageFileExecOptions;
	UCHAR BeingDebugged;
	UCHAR BitField;
	PVOID Mutant;
	PVOID ImageBaseAddress;
	PPEB_LDR_DATA Ldr;
	PVOID ProcessParameters;
	PVOID SubSystemData;
	PVOID ProcessHeap;
	PVOID FastPebLock;
	PVOID AtlThunkSListPtr;
	PVOID IFEOKey;
	PVOID CrossProcessFlags;
	PVOID KernelCallbackTable;
	ULONG SystemReserved;
	ULONG AtlThunkSListPtr32;
	PVOID ApiSetMap;
} PEB, * PPEB;

typedef struct _LDR_DATA_TABLE_ENTRY
{
	LIST_ENTRY InLoadOrderLinks;
	LIST_ENTRY InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;
	USHORT LoadCount;
	USHORT TlsIndex;
	LIST_ENTRY HashLinks;
	ULONG TimeDateStamp;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;



extern "C"
{
	// ָ�������������͵�ȫ��ָ�룬���ڱ�ʶ�����������
	POBJECT_TYPE* IoDriverObjectType;

	// ���ں�ģʽ�´����û�ģʽ�̣߳������ڽ���ע�룩
	NTSYSAPI NTSTATUS RtlCreateUserThread(
		HANDLE ProcessHandle,      // Ŀ����̾��
		PVOID SecurityDescriptor,  // ��ȫ������
		BOOLEAN CreateSuspended,   // �Ƿ���𴴽�
		ULONG StackZeroBits,       // ջ��������λ
		SIZE_T StackReserve,       // ջ������С
		SIZE_T StackCommit,        // ջ�ύ��С
		PVOID StartAddress,        // �߳���ں���
		PVOID Parameter,           // ���ݸ��̵߳Ĳ���
		PHANDLE ThreadHandle,      // ���ص��߳̾��
		PCLIENT_ID ClientId        // ���ص��߳�ID
	);

	// �ȴ�����ں˶������¼����ź�������Ϊ���ź�״̬
	__declspec(dllimport) NTSTATUS ZwWaitForMultipleObjects(
		unsigned long Count,       // ��������
		HANDLE Handles[],          // ����������
		WAIT_TYPE WaitType,        // �ȴ����ͣ�WaitAll/WaitAny��
		BOOLEAN Alertable,         // �Ƿ�ɾ���
		LARGE_INTEGER* Timeout     // ��ʱʱ��
	);

	// ��ȡָ�����̵Ľ��̻�����(PEB)ָ�루����������Ϣ��
	__declspec(dllimport) PPEB PsGetProcessPeb(PEPROCESS Process);

	// ͨ������������ģ���е����ĺ�����ַ�����ں˺�����
	__declspec(dllimport) void* __stdcall RtlFindExportedRoutineByName(
		void* ModuleBase,          // ģ���ַ
		PCCH RoutineName           // ��������
	);

	// ͨ���������ƣ����豸������ȡ�ں˶��������
	NTSYSAPI NTSTATUS NTAPI ObReferenceObjectByName(
		_In_ PUNICODE_STRING ObjectName,      // ����·����
		_In_ ULONG Attributes,                // ��������
		_In_opt_ PACCESS_STATE AccessState,   // ����״̬
		_In_opt_ ACCESS_MASK DesiredAccess,   // �����ķ���Ȩ��
		_In_ POBJECT_TYPE ObjectType,         // ��������
		_In_ KPROCESSOR_MODE AccessMode,      // ����ģʽ
		_Inout_opt_ PVOID ParseContext,       // ����������
		_Out_ PVOID* Object                   // ���صĶ���ָ��
	);

	// �����ں���������ע���ʼ�����������������ã�
	NTKERNELAPI NTSTATUS IoCreateDriver(
		PUNICODE_STRING DriverName,            // �������ƣ���ѡ��
		PDRIVER_INITIALIZE InitializationFunction  // ������ʼ������
	);
}