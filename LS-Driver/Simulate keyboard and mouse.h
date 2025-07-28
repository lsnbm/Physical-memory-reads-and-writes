//�ҵ����������Ļص�������ַ���豸�����ַ��������ʵ��ģ��

#pragma once
#include<ntifs.h>
#include <ntddmou.h>   // ��� �ṹ�� MOUSE_INPUT_DATA
#include <ntddkbd.h>   // ���� �ṹ��KEYBOARD_INPUT_DATA
#include <minwindef.h>


typedef VOID(*MY_KEYBOARDCALLBACK) (PDEVICE_OBJECT, PKEYBOARD_INPUT_DATA, PKEYBOARD_INPUT_DATA, PULONG);

typedef VOID(*MY_MOUSECALLBACK) (PDEVICE_OBJECT, PMOUSE_INPUT_DATA, PMOUSE_INPUT_DATA, PULONG);

extern "C"  POBJECT_TYPE* IoDriverObjectType;



static PDEVICE_OBJECT MouseDeviceObject = NULL;

static PDEVICE_OBJECT KeyboardDeviceObject = NULL;

static MY_MOUSECALLBACK MouseClassServiceCallback = NULL;

static MY_KEYBOARDCALLBACK KeyboardClassServiceCallback = NULL;


inline auto RtlGetSystemFun(LPWSTR Name) {

	UNICODE_STRING RoutineName;

	RtlInitUnicodeString(&RoutineName, Name);

	return MmGetSystemRoutineAddress(&RoutineName);
}

inline auto ZwReferenceObjectByName(PUNICODE_STRING ObjectName, ULONG Attributes, PACCESS_STATE PassedAccessState, ACCESS_MASK DesiredAccess, POBJECT_TYPE ObjectType, KPROCESSOR_MODE AccessMode, LPVOID ParseContext, PDRIVER_OBJECT* Object) -> NTSTATUS {

	typedef NTSTATUS(NTAPI* fn_ObReferenceObjectByName)(PUNICODE_STRING, ULONG, PACCESS_STATE, ACCESS_MASK, POBJECT_TYPE, KPROCESSOR_MODE, LPVOID, PDRIVER_OBJECT*);

	static fn_ObReferenceObjectByName _ObReferenceObjectByName = NULL;

	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	if (!_ObReferenceObjectByName|| !IoDriverObjectType) {

		//DbgPrint("ȷ��û�п�ʼ��ȡ\n");
		_ObReferenceObjectByName = (fn_ObReferenceObjectByName)(RtlGetSystemFun((LPWSTR)L"ObReferenceObjectByName"));


		//DbgPrint("1��ַ=%p\n", RtlGetSystemFun((LPWSTR)L"ObReferenceObjectByName"));//�����������:1��ַ=FFFFF8047E307D70

	}

	if (_ObReferenceObjectByName&& IoDriverObjectType) {
	//	DbgPrint("ȷ�ϻ�ȡ�ɹ�\n");
	//	DbgPrint("��ǰIRQL = %u\n", KeGetCurrentIrql());

	Status = _ObReferenceObjectByName(ObjectName, Attributes, PassedAccessState, DesiredAccess, ObjectType, AccessMode, ParseContext, Object);
	}

	return Status;
}

auto SearchServiceFromMouExt(PDRIVER_OBJECT MouDriverObject, PDEVICE_OBJECT pPortDev) -> NTSTATUS {

	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	PDEVICE_OBJECT pTargetDeviceObject = NULL;

	UCHAR* DeviceExt = NULL;

	LPVOID KbdDriverStart = NULL;

	ULONG KbdDriverSize = NULL;

	PDEVICE_OBJECT pTmpDev = NULL;

	UNICODE_STRING kbdDriName = { NULL };

	KbdDriverStart = MouDriverObject->DriverStart;

	KbdDriverSize = MouDriverObject->DriverSize;

	RtlInitUnicodeString(&kbdDriName, L"\\Driver\\mouclass");

	pTmpDev = pPortDev;

	while (pTmpDev->AttachedDevice != NULL) {

		if (RtlCompareUnicodeString(&pTmpDev->AttachedDevice->DriverObject->DriverName, &kbdDriName, TRUE)) {

			pTmpDev = pTmpDev->AttachedDevice;
		}
		else
			break;
	}

	if (pTmpDev->AttachedDevice != NULL) {

		pTargetDeviceObject = MouDriverObject->DeviceObject;

		while (pTargetDeviceObject) {

			if (pTmpDev->AttachedDevice != pTargetDeviceObject) {

				pTargetDeviceObject = pTargetDeviceObject->NextDevice;

				continue;
			}

			DeviceExt = (UCHAR*)pTmpDev->DeviceExtension;

			MouseDeviceObject = NULL;

			for (ULONG i = NULL; i < PAGE_SIZE; i++, DeviceExt++) {

				if (MmIsAddressValid(DeviceExt)) {

					LPVOID pTemp = *(LPVOID*)DeviceExt;

					if (MouseDeviceObject && MouseClassServiceCallback) {

						Status = STATUS_SUCCESS;

						break;
					}

					if (pTemp == pTargetDeviceObject) {

						MouseDeviceObject = pTargetDeviceObject;

						continue;
					}

					if (pTemp > KbdDriverStart && pTemp < (LPVOID)((UCHAR*)KbdDriverStart + KbdDriverSize) && MmIsAddressValid(pTemp)) {

						MouseClassServiceCallback = (MY_MOUSECALLBACK)pTemp;

						Status = STATUS_SUCCESS;
					}
				}
				else
					break;
			}

			if (Status == STATUS_SUCCESS) {

				break;
			}

			pTargetDeviceObject = pTargetDeviceObject->NextDevice;
		}
	}

	return Status;
}
auto SearchServiceFromKdbExt(PDRIVER_OBJECT KbdDriverObject, PDEVICE_OBJECT pPortDev) -> NTSTATUS {

	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	PDEVICE_OBJECT pTargetDeviceObject = NULL;

	UCHAR* DeviceExt = NULL;

	LPVOID KbdDriverStart = NULL;

	ULONG KbdDriverSize = NULL;

	PDEVICE_OBJECT pTmpDev = NULL;

	UNICODE_STRING kbdDriName = { NULL };

	KbdDriverStart = KbdDriverObject->DriverStart;

	KbdDriverSize = KbdDriverObject->DriverSize;

	RtlInitUnicodeString(&kbdDriName, L"\\Driver\\kbdclass");

	pTmpDev = pPortDev;

	while (pTmpDev->AttachedDevice != NULL) {

		if (RtlCompareUnicodeString(&pTmpDev->AttachedDevice->DriverObject->DriverName, &kbdDriName, TRUE)) {

			pTmpDev = pTmpDev->AttachedDevice;
		}
		else
			break;
	}

	if (pTmpDev->AttachedDevice != NULL) {

		pTargetDeviceObject = KbdDriverObject->DeviceObject;

		while (pTargetDeviceObject) {

			if (pTmpDev->AttachedDevice != pTargetDeviceObject) {

				pTargetDeviceObject = pTargetDeviceObject->NextDevice;

				continue;
			}

			DeviceExt = (UCHAR*)pTmpDev->DeviceExtension;

			KeyboardDeviceObject = NULL;

			for (ULONG i = NULL; i < PAGE_SIZE; i++, DeviceExt++) {

				if (MmIsAddressValid(DeviceExt)) {

					LPVOID pTemp = *(LPVOID*)DeviceExt;

					if (KeyboardDeviceObject && KeyboardClassServiceCallback) {

						Status = STATUS_SUCCESS;

						break;
					}

					if (pTemp == pTargetDeviceObject) {

						KeyboardDeviceObject = pTargetDeviceObject;

						continue;
					}

					if (pTemp > KbdDriverStart && pTemp < (LPVOID)((UCHAR*)KbdDriverStart + KbdDriverSize) && MmIsAddressValid(pTemp)) {

						KeyboardClassServiceCallback = (MY_KEYBOARDCALLBACK)pTemp;
					}
				}
				else
					break;
			}

			if (Status == STATUS_SUCCESS) {

				break;
			}

			pTargetDeviceObject = pTargetDeviceObject->NextDevice;
		}
	}

	return Status;
}

auto SearchMouServiceCallBack() -> NTSTATUS {

	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	PDRIVER_OBJECT ClassObject = NULL;

	PDRIVER_OBJECT DriverObject = NULL;

	PDEVICE_OBJECT DeviceObject = NULL;

	UNICODE_STRING DeviceName[] = { RTL_CONSTANT_STRING(L"\\Driver\\mouhid"), RTL_CONSTANT_STRING(L"\\Driver\\i8042prt") };

	for (size_t i = NULL; i < ARRAYSIZE(DeviceName); i++) {

		Status = ZwReferenceObjectByName(&DeviceName[i], OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, &DriverObject);

		if (NT_SUCCESS(Status)) {

			ObfDereferenceObject(DriverObject);

			break;
		}
	}

	if (DriverObject != NULL) {

		UNICODE_STRING ClassName = RTL_CONSTANT_STRING(L"\\Driver\\mouclass");

		Status = ZwReferenceObjectByName(&ClassName, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, &ClassObject);

		if (NT_SUCCESS(Status)) {

			DeviceObject = DriverObject->DeviceObject;

			while (DeviceObject) {

				Status = SearchServiceFromMouExt(ClassObject, DeviceObject);

				if (!NT_SUCCESS(Status)) {

					DeviceObject = DeviceObject->NextDevice;
				}
				else
					break;
			}

			ObfDereferenceObject(ClassObject);
		}
	}

	return Status;
}
auto SearchKdbServiceCallBack() -> NTSTATUS {

	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	PDRIVER_OBJECT ClassObject = NULL;

	PDRIVER_OBJECT DriverObject = NULL;

	PDEVICE_OBJECT DeviceObject = NULL;

	UNICODE_STRING DeviceName[] = { RTL_CONSTANT_STRING(L"\\Driver\\kbdhid"), RTL_CONSTANT_STRING(L"\\Driver\\i8042prt") };

	for (size_t i = NULL; i < ARRAYSIZE(DeviceName); i++) {

		Status = ZwReferenceObjectByName(&DeviceName[i], OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, &DriverObject);

		if (NT_SUCCESS(Status)) {

			ObfDereferenceObject(DriverObject);

			break;
		}
	}

	if (DriverObject != NULL) {

		UNICODE_STRING ClassName = RTL_CONSTANT_STRING(L"\\Driver\\kbdclass");

		Status = ZwReferenceObjectByName(&ClassName, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, &ClassObject);

		if (NT_SUCCESS(Status)) {

			DeviceObject = DriverObject->DeviceObject;

			while (DeviceObject) {

				Status = SearchServiceFromKdbExt(ClassObject, DeviceObject);

				if (!NT_SUCCESS(Status)) {

					DeviceObject = DeviceObject->NextDevice;
				}
				else
					break;
			}

			ObfDereferenceObject(ClassObject);
		}
	}

	return Status;
}


// ��ʼ������ģ��ģ��
inline NTSTATUS InitMouseAndKeyboard()
{
	//��鵱ǰIRQLȷ����ʼ���� 0��PASSIVE_LEVEL��
	KIRQL irql = KeGetCurrentIrql();
	DbgPrint("InitMouseAndKeyboard called at IRQL = %d\n", irql);

	if(irql != 0) return 0;

	//��ȡIoDriverObjectType�ĵ�ַ(�����ȡ!!!)
	IoDriverObjectType = (POBJECT_TYPE*)(RtlGetSystemFun((LPWSTR)L"IoDriverObjectType"));

	//����豸�ͻص�����ָ���ʼ��
	if (MouseDeviceObject == NULL || MouseClassServiceCallback == NULL) {

		NTSTATUS mouStatus = SearchMouServiceCallBack();
		if (NT_SUCCESS(mouStatus))
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[+] Mouse simulation initialized successfully.\n");
		}
		else
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[-] Failed to initialize mouse simulation. Status: 0x%X\n", mouStatus);
		}
	}
	//�����豸�ͻص�����ָ���ʼ��
	if (KeyboardDeviceObject == NULL || KeyboardClassServiceCallback == NULL) {

		NTSTATUS kbdStatus = SearchKdbServiceCallBack();
		if (NT_SUCCESS(kbdStatus))
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[+] Keyboard simulation initialized successfully.\n");
		}
		else
		{
			DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[-] Failed to initialize keyboard simulation. Status: 0x%X\n", kbdStatus);
		}

	
	}

	return (MouseDeviceObject && MouseClassServiceCallback && KeyboardDeviceObject && KeyboardClassServiceCallback) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}



// [����] ����ģ�⺯��
inline void KeyboardSimulation(PKEYBOARD_INPUT_DATA Data)
{
	if (!KeyboardClassServiceCallback || !KeyboardDeviceObject || !Data) {
		return;
	}

	ULONG InputDataConsumed = 0; 

	// [����] ֱ��ʹ�õ����ߴ����ָ�룬�����Ǵ����ֲ�����
	PKEYBOARD_INPUT_DATA KbdInputDataStart = Data;
	PKEYBOARD_INPUT_DATA KbdInputDataEnd = KbdInputDataStart + 1;

	// ���ûص�����
	KeyboardClassServiceCallback(KeyboardDeviceObject, KbdInputDataStart, KbdInputDataEnd, &InputDataConsumed);
}

// [����] ���ģ�⺯��
inline void MouseSimulation(PMOUSE_INPUT_DATA Data)
{
	if (!MouseClassServiceCallback || !MouseDeviceObject || !Data) {
		return;
	}

	ULONG InputDataConsumed = 0; 

	// [����] ֱ��ʹ�õ����ߴ����ָ��
	PMOUSE_INPUT_DATA MouseInputDataStart = Data;
	PMOUSE_INPUT_DATA MouseInputDataEnd = MouseInputDataStart + 1;

	// ���ûص�����
	MouseClassServiceCallback(MouseDeviceObject, MouseInputDataStart, MouseInputDataEnd, &InputDataConsumed);
}