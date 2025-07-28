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
	// 指向驱动对象类型的全局指针，用于标识驱动程序对象
	POBJECT_TYPE* IoDriverObjectType;

	// 在内核模式下创建用户模式线程（常用于进程注入）
	NTSYSAPI NTSTATUS RtlCreateUserThread(
		HANDLE ProcessHandle,      // 目标进程句柄
		PVOID SecurityDescriptor,  // 安全描述符
		BOOLEAN CreateSuspended,   // 是否挂起创建
		ULONG StackZeroBits,       // 栈保留的零位
		SIZE_T StackReserve,       // 栈保留大小
		SIZE_T StackCommit,        // 栈提交大小
		PVOID StartAddress,        // 线程入口函数
		PVOID Parameter,           // 传递给线程的参数
		PHANDLE ThreadHandle,      // 返回的线程句柄
		PCLIENT_ID ClientId        // 返回的线程ID
	);

	// 等待多个内核对象（如事件、信号量）变为有信号状态
	__declspec(dllimport) NTSTATUS ZwWaitForMultipleObjects(
		unsigned long Count,       // 对象数量
		HANDLE Handles[],          // 对象句柄数组
		WAIT_TYPE WaitType,        // 等待类型（WaitAll/WaitAny）
		BOOLEAN Alertable,         // 是否可警告
		LARGE_INTEGER* Timeout     // 超时时间
	);

	// 获取指定进程的进程环境块(PEB)指针（包含进程信息）
	__declspec(dllimport) PPEB PsGetProcessPeb(PEPROCESS Process);

	// 通过函数名查找模块中导出的函数地址（如内核函数）
	__declspec(dllimport) void* __stdcall RtlFindExportedRoutineByName(
		void* ModuleBase,          // 模块基址
		PCCH RoutineName           // 函数名称
	);

	// 通过对象名称（如设备名）获取内核对象的引用
	NTSYSAPI NTSTATUS NTAPI ObReferenceObjectByName(
		_In_ PUNICODE_STRING ObjectName,      // 对象路径名
		_In_ ULONG Attributes,                // 对象属性
		_In_opt_ PACCESS_STATE AccessState,   // 访问状态
		_In_opt_ ACCESS_MASK DesiredAccess,   // 期望的访问权限
		_In_ POBJECT_TYPE ObjectType,         // 对象类型
		_In_ KPROCESSOR_MODE AccessMode,      // 访问模式
		_Inout_opt_ PVOID ParseContext,       // 解析上下文
		_Out_ PVOID* Object                   // 返回的对象指针
	);

	// 创建内核驱动对象并注册初始化函数（驱动开发用）
	NTKERNELAPI NTSTATUS IoCreateDriver(
		PUNICODE_STRING DriverName,            // 驱动名称（可选）
		PDRIVER_INITIALIZE InitializationFunction  // 驱动初始化函数
	);
}