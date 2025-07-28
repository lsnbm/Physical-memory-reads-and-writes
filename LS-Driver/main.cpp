#include"driver.hpp"



/*
static ȫ����Ҫ���þ���:
	1.��������ݶ�(��̬�洢��)
	2.�ڲ�����(�޷�ʹ��externҲ�޷�����)
	3.��������Ϊ�������������ڼ�
static �ֲ���Ҫ���þ���:
	1.��������ݶ�(��̬�洢��)
	2.�ڲ�����(�޷�ʹ��externҲ�޷�����)
	3.��������Ϊ�������������ڼ�
	4.���������ó�ʼ��һ�κ�������
*/

/*
����				inline __forceinline										static	inline __forceinline
������				�ڲ����� (Internal Linkage)									�ⲿ���� (External Linkage)
�ɼ���				���ڵ�ǰ���뵥Ԫ (.cpp �ļ�)�ڿɼ�							�����������пɼ�
��α������Ӵ���	ÿ���ļ�����һ��˽�и�������������������					inline �ؼ��ָ�֪�������ϲ������ͬ�Ķ���
ʹ�ó���			C ���Է��C/C++ �ж��ǳ���ȫ�ɿ�	�ִ� C++ ���			������ͷ�ļ���ʵ�ֺ�����
����˼��			��Ϊ���Լ����ļ�����һ�����ظ�����							������һ��ȫ�ֺ����������������ж����ͬ�Ķ��塱

*/

NTSTATUS DriverEntry(
	PDRIVER_OBJECT  pDriverObject,/*����������ʵ����������������������������ṹ*/
	PUNICODE_STRING pRegistryPath/*����ע���·��*/
)
{
	//����һ���꣬���ڸ��߱������������δ��ʹ�ã��Ա������������δʹ�ò����ľ���
	UNREFERENCED_PARAMETER(pRegistryPath);

	DbgPrint("Driver:%wZ\n", pRegistryPath);
	NTSTATUS status;
	static UNICODE_STRING devName;//����һ��UNICODE_STRING�ṹ�壬���ڴ洢�豸���ơ�
	static UNICODE_STRING symLink;//�����������ӣ��û�ģʽ����ͨ�������ӷ����豸
	// �����豸����
	RtlInitUnicodeString(&devName, L"\\Device\\liaoStarDriver");              //��̬��ʼ���豸����
	RtlInitUnicodeString(&symLink, L"\\DosDevices\\liaoStarDriver");          //��̬��ʼ����������


	PDEVICE_OBJECT pDeviceObject = NULL;//����һ���豸����ָ��
	status = IoCreateDevice(
		pDriverObject,            // �������󣬿��Գ���Ϊ�������������豸����
		00,                      // �豸��չ��С(���Ƿ����ڴ�ռ䣬0��ʾ�������ڴ棬200��ʾ����200�ֽ�)
		&devName,               // �豸����
		FILE_DEVICE_UNKNOWN,   // �豸����
		0,                    // �豸����
		TRUE,               // �豸�Ƿ��ռ
		&pDeviceObject      // �����豸����ָ��
	);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to create device object:%x\n", status);
		return status;
	}
	DbgPrint("Created DeviceObject successfully\n");

	status = IoCreateSymbolicLink(&symLink, &devName);//�����������ӷ���״̬��
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to create symbolic link:%x\n", status);//��ӡ����״̬����Ϣ
		IoDeleteDevice(pDeviceObject);//ɾ���豸����
		return status;
	}
	DbgPrint("Symbolic link created successfully\n");




	//����������Ϊ�����������ģʽ
   // SetFlag(pDeviceObject->Flags, DO_BUFFERED_IO);
	// ֱ��IOģʽ
	//SetFlag(pDeviceObject->Flags, DO_DIRECT_IO);

	 // ���߲������κα�־������ʹ��METHOD_NEITHER





	// �����û����豸������
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCC;
	// �����û��ر��豸������
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceCC;

	//���ر��ļ������Ͽ����豸������ʱ,֪ͨ����������б�Ҫ���������Ļص�����
	pDriverObject->MajorFunction[IRP_MJ_CLEANUP] = DeviceCC;

	// �����û�����ж�ش�����
	pDriverObject->DriverUnload = DriverUnload;
	// ���ô����豸����������
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;
	/*
	��ϸ���ͣ�
		���û�������豸ʱ����ϵͳ������IRP_MJ_CREATE���󣬲����ݸ�����������
		�����յ�����ʱ�����MajorFunction[IRP_MJ_CREATE] ��ע��һ����������������
		�Ὣ�豸����Ϊ��״̬��
		�������ʾ����û�д����κθ����߼�(����Ȩ�ޣ���Դ�����).
	*/


	return status;
}
