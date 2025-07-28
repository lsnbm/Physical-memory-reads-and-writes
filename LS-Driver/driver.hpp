#pragma once
#include<ntifs.h>
#include <ntddmou.h>   // ��� �ṹ�� MOUSE_INPUT_DATA
#include <ntddkbd.h>   // ���� �ṹ��KEYBOARD_INPUT_DATA

//����IOCTL������
#define IOCTL_READ_MEMORY           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9000, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY          CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9001, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE_BASE       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9002, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_TERMINATE_PROCESS     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9003, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_SIMULATE_KEYBOARD		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9004, METHOD_NEITHER, FILE_ANY_ACCESS) 
#define IOCTL_SIMULATE_MOUSE		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9005, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_HANDLE_ELEVATION		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9006, METHOD_NEITHER, FILE_ANY_ACCESS)
#define IOCTL_DUMP					CTL_CODE(FILE_DEVICE_UNKNOWN, 0x9007, METHOD_NEITHER, FILE_ANY_ACCESS)


#define IOCTL_NULL_IO     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8000, METHOD_NEITHER, FILE_ANY_ACCESS)
//���䷽��         	�������ݴ���	            ������ݴ���	              ��ȫ��			����	���ó���
//METHOD_BUFFERED	�Զ����Ƶ��ں˻�����	    �Զ����ں˻���������	        ��    			��	    С����������ȫҪ���
//METHOD_IN_DIRECT	MDL ӳ�䣨�޸��ƣ�	        �������Ƶ��û��ռ�              ��				�и�	����������
//METHOD_OUT_DIRECT	�Զ����Ƶ��ں˻�����	    MDL ӳ�䣨�޸��ƣ�	            ��				�и�	���������
//METHOD_NEITHER	ֱ��ʹ���û�ָ�루����֤��	ֱ��ʹ���û�ָ�루����֤��      �ͣ����ֶ���֤�� ��		�����ܡ������λ���



extern "C" {


	//��ʼ������
	NTSTATUS DriverEntry(PDRIVER_OBJECT  pDriverObject, PUNICODE_STRING pRegistryPath);
	NTSTATUS DeviceCC(PDEVICE_OBJECT pDeviceObject, PIRP irp);
	NTSTATUS DriverDeviceControl(PDEVICE_OBJECT pDeviceObject, PIRP irp);
	VOID     DriverUnload(PDRIVER_OBJECT pDrivereObject);


	// BugCheck����
	VOID KeBugCheckEx(ULONG BugCheckCode, ULONG_PTR BugCheckParameter1, ULONG_PTR BugCheckParameter2, ULONG_PTR BugCheckParameter3, ULONG_PTR BugCheckParameter4);

}

struct DriverPacket {
	//�ڴ��ȡ
	UINT32      TargetProcessId;
	PVOID       TargetAddress;
	PVOID       UserBufferAddress;
	ULONG       TransferSize;

	//ģ�����ַ��ȡ
	const char* ModuleName;
	ULONG64     ModuleBaseAddress;
	ULONG64     ModuleSize;

	//ģ����̺��������
	MOUSE_INPUT_DATA    MouseData;
	KEYBOARD_INPUT_DATA KeyboardData;

	//Ҫ�����ľ��
	HANDLE    ProcessHandle;
};

