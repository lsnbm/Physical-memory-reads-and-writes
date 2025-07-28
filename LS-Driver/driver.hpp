#pragma once
#include<ntifs.h>
#include <ntddmou.h>   // 鼠标 结构体 MOUSE_INPUT_DATA
#include <ntddkbd.h>   // 键盘 结构体KEYBOARD_INPUT_DATA

//定义IOCTL控制码
#define IOCTL_READ_MEMORY           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9000, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9001, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE_BASE       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9002, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_TERMINATE_PROCESS     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9003, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_SIMULATE_KEYBOARD		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9004, METHOD_NEITHER, FILE_ANY_ACCESS) 
#define IOCTL_SIMULATE_MOUSE		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9005, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_HANDLE_ELEVATION		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9006, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_DUMP					CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9007, METHOD_NEITHER, FILE_ANY_ACCESS)


#define IOCTL_NULL_IO     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8000, METHOD_NEITHER, FILE_ANY_ACCESS)
//传输方法         	输入数据处理	            输出数据处理	              安全性			性能	适用场景
//METHOD_BUFFERED	自动复制到内核缓冲区	    自动从内核缓冲区复制	        高    			低	    小数据量、安全要求高
//METHOD_IN_DIRECT	MDL 映射（无复制）	        驱动复制到用户空间              中				中高	大数据输入
//METHOD_OUT_DIRECT	自动复制到内核缓冲区	    MDL 映射（无复制）	            中				中高	大数据输出
//METHOD_NEITHER	直接使用用户指针（需验证）	直接使用用户指针（需验证）      低（需手动验证） 高		高性能、可信任环境



extern "C" {


	//初始化驱动
	NTSTATUS DriverEntry(PDRIVER_OBJECT  pDriverObject, PUNICODE_STRING pRegistryPath);
	NTSTATUS DeviceCC(PDEVICE_OBJECT pDeviceObject, PIRP irp);
	NTSTATUS DriverDeviceControl(PDEVICE_OBJECT pDeviceObject, PIRP irp);
	VOID     DriverUnload(PDRIVER_OBJECT pDrivereObject);


	// BugCheck函数
	VOID KeBugCheckEx(ULONG BugCheckCode, ULONG_PTR BugCheckParameter1, ULONG_PTR BugCheckParameter2, ULONG_PTR BugCheckParameter3, ULONG_PTR BugCheckParameter4);

}

struct DriverPacket {
	//内存读取
	UINT32      TargetProcessId;
	PVOID       TargetAddress;
	PVOID       UserBufferAddress;
	ULONG       TransferSize;

	//模块基地址获取
	const char* ModuleName;
	ULONG64     ModuleBaseAddress;
	ULONG64     ModuleSize;

	//模拟键盘和鼠标输入
	MOUSE_INPUT_DATA    MouseData;
	KEYBOARD_INPUT_DATA KeyboardData;

	//要提升的句柄
	HANDLE    ProcessHandle;
};

