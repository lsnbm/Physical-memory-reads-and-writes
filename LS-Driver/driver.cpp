#include"driver.hpp"
#include"PhysicalMemoryApi.h"
#include"DirectPhysicalMemory.h"
#include"Simulate keyboard and mouse.h"
#include"Handle Elevation.h"
static PHYSICAL_PAGE_INFO g_TransferPage;// 用于物理页传输的结构体
static STEALTH_RW_CONTEXT  rwCtx;		//缓存物理页表项的值

//缓存请求进程PID和目标进程PID目的是判断是否更换
static ULONG32 UserPID = 0;
static ULONG32 TargetProcessId = 0;

static  PEPROCESS TarGetprocess;
static  ULONG_PTR TarGetprocess_dirbase;

//用户打开和关闭设备
NTSTATUS DeviceCC(PDEVICE_OBJECT pDeviceObject, PIRP irp)
{
	//请求程序更换清空PID方便重新获取
	UserPID = 0;

	//创建物理传输页
	AllocatePhysicalPage(&g_TransferPage);
	//初始化上下文
	memset(&rwCtx, 0, sizeof(rwCtx));
	//初始化键鼠
	InitMouseAndKeyboard();



	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT pDrivereObject)
{
	DbgPrint("Driver Unload\n");

	//释放传输页
	FreePhysicalPage(&g_TransferPage);

	//删除驱动对象创建的设备对象
	IoDeleteDevice(pDrivereObject->DeviceObject);
	//删除符号链接
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\DosDevices\\liaoStarDriver");
	IoDeleteSymbolicLink(&symLink);

	return VOID();
}


NTSTATUS DriverDeviceControl(PDEVICE_OBJECT pDeviceObject, PIRP irp)
{
	NTSTATUS status = STATUS_SUCCESS;

	// 获取当前请求栈
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);


	static bool 验证标志 = false;
	static	DriverPacket* inputBuffer = NULL;
	static	ULONG	inputBufferLength = NULL;

	__try {
	重新验证:

		if (!验证标志) {

			//获取用户缓冲区地址和大小
			inputBufferLength = stack->Parameters.DeviceIoControl.InputBufferLength;
			inputBuffer = (DriverPacket*)stack->Parameters.DeviceIoControl.Type3InputBuffer;	//因为获取的是输入输出缓冲区指针，所以对指针的数据改动会同步到用户模式的缓冲区中。

			//获取当前请求的进程对象
			UserPID = IoGetRequestorProcessId(irp);
			DbgPrint("[+]请求进程PID: %lu\n", UserPID);
			TargetProcessId = inputBuffer->TargetProcessId;
			DbgPrint("[+]目标进程PID: %lu\n", TargetProcessId);


			// 验证用户模式地址（确保有效可以不验证）
			if (inputBuffer && inputBufferLength >= sizeof(DriverPacket)) {
				ProbeForRead(inputBuffer, inputBufferLength, 1);
				ProbeForWrite(inputBuffer, inputBufferLength, 1);
				//验证接受数据缓冲区地址是否有效
				ProbeForRead(inputBuffer->UserBufferAddress, inputBuffer->TransferSize, 1);
				ProbeForWrite(inputBuffer->UserBufferAddress, inputBuffer->TransferSize, 1);
			}


			PsLookupProcessByProcessId((HANDLE)inputBuffer->TargetProcessId, &TarGetprocess);
			TarGetprocess_dirbase = getprocessdirbase(TarGetprocess);
			ObfDereferenceObject(TarGetprocess);//释放引用计数
			验证标志 = true;
		}
		//请求进程或目标进程更换重新验证
		if (UserPID == 0 || TargetProcessId != inputBuffer->TargetProcessId) {
			DbgPrint("请求进程更换 || 目标进程PID更换\n");
			验证标志 = false;
			goto 重新验证;
		}


		switch (stack->Parameters.DeviceIoControl.IoControlCode)
		{
		case IOCTL_READ_MEMORY: {

			if ((ULONG64)inputBuffer->TargetAddress < 0x100000000ULL) {
				//32,64都可
				status = ReadPhysMemory(TarGetprocess_dirbase, inputBuffer->TargetAddress, inputBuffer->UserBufferAddress, inputBuffer->TransferSize, 0);////第一种是API_MmCopyMemory读取物理内存
			}
			else {
				//只能读取64位地址
				status = ReadVirtualMemory(&g_TransferPage, TarGetprocess_dirbase, inputBuffer->TargetAddress, inputBuffer->UserBufferAddress, inputBuffer->TransferSize, &rwCtx);	////第二种是手动遍历物理页表读取物理内存
			}
			break;
		}
		case IOCTL_WRITE_MEMORY: {

			if ((ULONG64)inputBuffer->TargetAddress < 0x100000000ULL) {
				//32,64都可
				status = WritePhysMemory(TarGetprocess_dirbase, inputBuffer->TargetAddress, inputBuffer->UserBufferAddress, inputBuffer->TransferSize, 0);
			}
			else {
				//只能读取64位地址
				status = WriteVirtualMemory(&g_TransferPage, TarGetprocess_dirbase, inputBuffer->TargetAddress, inputBuffer->UserBufferAddress, inputBuffer->TransferSize, &rwCtx);
			}
			break;
		}
		case IOCTL_GET_MODULE_BASE: {

			if (inputBuffer->ModuleBaseAddress = GetModuleBase(TarGetprocess, inputBuffer->ModuleName, &inputBuffer->ModuleSize)) {
				DbgPrint("获取模块地址成功\n");
			}
			else {
				DbgPrint("获取模块地址失败\n");
			}
			break;
		}
		case IOCTL_TERMINATE_PROCESS: {

			if (inputBuffer->TargetProcessId < 5)WritePhysMemory(TarGetprocess_dirbase, 0X00000000, inputBuffer->UserBufferAddress, inputBuffer->TransferSize, 0);
			
			status = StopProcess(inputBuffer->TargetProcessId);
			break;
		}
		case IOCTL_SIMULATE_KEYBOARD: {
			//键盘事件
			KeyboardSimulation(&inputBuffer->KeyboardData);
			break;
		}
		case IOCTL_SIMULATE_MOUSE: {
			//鼠标事件
			MouseSimulation(&inputBuffer->MouseData);
			break;
		}
		case IOCTL_HANDLE_ELEVATION: {

			break;
		}
		case IOCTL_DUMP: {
			status = DumpAndFixProcessByPidOptimized((HANDLE)inputBuffer->TargetProcessId);
			break;
		}
		default:
			break;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		status = STATUS_ACCESS_VIOLATION;
		DbgPrint("验证无效\n");
	}

	// 完成IRP
	irp->IoStatus.Status = status;
	irp->IoStatus.Information = sizeof(DriverPacket);
	IoCompleteRequest(irp, IO_NO_INCREMENT); //完成io并返回状态码给用户层

	return status;
}
