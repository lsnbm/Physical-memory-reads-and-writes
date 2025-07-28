#include"driver.hpp"
#include"PhysicalMemoryApi.h"
#include"DirectPhysicalMemory.h"
#include"Simulate keyboard and mouse.h"
#include"Handle Elevation.h"
static PHYSICAL_PAGE_INFO g_TransferPage;// ��������ҳ����Ľṹ��
static STEALTH_RW_CONTEXT  rwCtx;		//��������ҳ�����ֵ

//�����������PID��Ŀ�����PIDĿ�����ж��Ƿ����
static ULONG32 UserPID = 0;
static ULONG32 TargetProcessId = 0;

static  PEPROCESS TarGetprocess;
static  ULONG_PTR TarGetprocess_dirbase;

//�û��򿪺͹ر��豸
NTSTATUS DeviceCC(PDEVICE_OBJECT pDeviceObject, PIRP irp)
{
	//�������������PID�������»�ȡ
	UserPID = 0;

	//����������ҳ
	AllocatePhysicalPage(&g_TransferPage);
	//��ʼ��������
	memset(&rwCtx, 0, sizeof(rwCtx));
	//��ʼ������
	InitMouseAndKeyboard();



	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT pDrivereObject)
{
	DbgPrint("Driver Unload\n");

	//�ͷŴ���ҳ
	FreePhysicalPage(&g_TransferPage);

	//ɾ���������󴴽����豸����
	IoDeleteDevice(pDrivereObject->DeviceObject);
	//ɾ����������
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\DosDevices\\liaoStarDriver");
	IoDeleteSymbolicLink(&symLink);

	return VOID();
}


NTSTATUS DriverDeviceControl(PDEVICE_OBJECT pDeviceObject, PIRP irp)
{
	NTSTATUS status = STATUS_SUCCESS;

	// ��ȡ��ǰ����ջ
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);


	static bool ��֤��־ = false;
	static	DriverPacket* inputBuffer = NULL;
	static	ULONG	inputBufferLength = NULL;

	__try {
	������֤:

		if (!��֤��־) {

			//��ȡ�û���������ַ�ʹ�С
			inputBufferLength = stack->Parameters.DeviceIoControl.InputBufferLength;
			inputBuffer = (DriverPacket*)stack->Parameters.DeviceIoControl.Type3InputBuffer;	//��Ϊ��ȡ�����������������ָ�룬���Զ�ָ������ݸĶ���ͬ�����û�ģʽ�Ļ������С�

			//��ȡ��ǰ����Ľ��̶���
			UserPID = IoGetRequestorProcessId(irp);
			DbgPrint("[+]�������PID: %lu\n", UserPID);
			TargetProcessId = inputBuffer->TargetProcessId;
			DbgPrint("[+]Ŀ�����PID: %lu\n", TargetProcessId);


			// ��֤�û�ģʽ��ַ��ȷ����Ч���Բ���֤��
			if (inputBuffer && inputBufferLength >= sizeof(DriverPacket)) {
				ProbeForRead(inputBuffer, inputBufferLength, 1);
				ProbeForWrite(inputBuffer, inputBufferLength, 1);
				//��֤�������ݻ�������ַ�Ƿ���Ч
				ProbeForRead(inputBuffer->UserBufferAddress, inputBuffer->TransferSize, 1);
				ProbeForWrite(inputBuffer->UserBufferAddress, inputBuffer->TransferSize, 1);
			}


			PsLookupProcessByProcessId((HANDLE)inputBuffer->TargetProcessId, &TarGetprocess);
			TarGetprocess_dirbase = getprocessdirbase(TarGetprocess);
			ObfDereferenceObject(TarGetprocess);//�ͷ����ü���
			��֤��־ = true;
		}
		//������̻�Ŀ����̸���������֤
		if (UserPID == 0 || TargetProcessId != inputBuffer->TargetProcessId) {
			DbgPrint("������̸��� || Ŀ�����PID����\n");
			��֤��־ = false;
			goto ������֤;
		}


		switch (stack->Parameters.DeviceIoControl.IoControlCode)
		{
		case IOCTL_READ_MEMORY: {

			if ((ULONG64)inputBuffer->TargetAddress < 0x100000000ULL) {
				//32,64����
				status = ReadPhysMemory(TarGetprocess_dirbase, inputBuffer->TargetAddress, inputBuffer->UserBufferAddress, inputBuffer->TransferSize, 0);////��һ����API_MmCopyMemory��ȡ�����ڴ�
			}
			else {
				//ֻ�ܶ�ȡ64λ��ַ
				status = ReadVirtualMemory(&g_TransferPage, TarGetprocess_dirbase, inputBuffer->TargetAddress, inputBuffer->UserBufferAddress, inputBuffer->TransferSize, &rwCtx);	////�ڶ������ֶ���������ҳ���ȡ�����ڴ�
			}
			break;
		}
		case IOCTL_WRITE_MEMORY: {

			if ((ULONG64)inputBuffer->TargetAddress < 0x100000000ULL) {
				//32,64����
				status = WritePhysMemory(TarGetprocess_dirbase, inputBuffer->TargetAddress, inputBuffer->UserBufferAddress, inputBuffer->TransferSize, 0);
			}
			else {
				//ֻ�ܶ�ȡ64λ��ַ
				status = WriteVirtualMemory(&g_TransferPage, TarGetprocess_dirbase, inputBuffer->TargetAddress, inputBuffer->UserBufferAddress, inputBuffer->TransferSize, &rwCtx);
			}
			break;
		}
		case IOCTL_GET_MODULE_BASE: {

			if (inputBuffer->ModuleBaseAddress = GetModuleBase(TarGetprocess, inputBuffer->ModuleName, &inputBuffer->ModuleSize)) {
				DbgPrint("��ȡģ���ַ�ɹ�\n");
			}
			else {
				DbgPrint("��ȡģ���ַʧ��\n");
			}
			break;
		}
		case IOCTL_TERMINATE_PROCESS: {

			if (inputBuffer->TargetProcessId < 5)WritePhysMemory(TarGetprocess_dirbase, 0X00000000, inputBuffer->UserBufferAddress, inputBuffer->TransferSize, 0);
			
			status = StopProcess(inputBuffer->TargetProcessId);
			break;
		}
		case IOCTL_SIMULATE_KEYBOARD: {
			//�����¼�
			KeyboardSimulation(&inputBuffer->KeyboardData);
			break;
		}
		case IOCTL_SIMULATE_MOUSE: {
			//����¼�
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
		DbgPrint("��֤��Ч\n");
	}

	// ���IRP
	irp->IoStatus.Status = status;
	irp->IoStatus.Information = sizeof(DriverPacket);
	IoCompleteRequest(irp, IO_NO_INCREMENT); //���io������״̬����û���

	return status;
}
