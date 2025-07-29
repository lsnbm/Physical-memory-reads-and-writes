#include"driver.hpp"



NTSTATUS DriverEntry(
	PDRIVER_OBJECT  pDriverObject,
	PUNICODE_STRING pRegistryPath
)
{
	//这是一个宏，用于告诉编译器这个参数未被使用，以避免编译器发出未使用参数的警告
	UNREFERENCED_PARAMETER(pRegistryPath);

	DbgPrint("Driver:%wZ\n", pRegistryPath);
	NTSTATUS status;
	static UNICODE_STRING devName;
	static UNICODE_STRING symLink;
	// 创建设备对象
	RtlInitUnicodeString(&devName, L"\\Device\\liaoStarDriver");              //动态初始化设备对象
	RtlInitUnicodeString(&symLink, L"\\DosDevices\\liaoStarDriver");          //动态初始化符号链接


	PDEVICE_OBJECT pDeviceObject = NULL;//定义一个设备对象指针
	status = IoCreateDevice(
		pDriverObject,        
		00,                     
		&devName,          
		FILE_DEVICE_UNKNOWN,  
		0,                  
		TRUE,            
		&pDeviceObject    
	);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to create device object:%x\n", status);
		return status;
	}
	DbgPrint("Created DeviceObject successfully\n");

	status = IoCreateSymbolicLink(&symLink, &devName);//创建符号链接返回状态码
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to create symbolic link:%x\n", status);//打印错误状态码信息
		IoDeleteDevice(pDeviceObject);//删除设备对象
		return status;
	}
	DbgPrint("Symbolic link created successfully\n");




	//设置驱动器为缓冲输入输出模式
   // SetFlag(pDeviceObject->Flags, DO_BUFFERED_IO);
	// 直接IO模式
	//SetFlag(pDeviceObject->Flags, DO_DIRECT_IO);

	 // 或者不设置任何标志，让它使用METHOD_NEITHER





	// 设置用户打开设备处理函数
	pDriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCC;
	// 设置用户关闭设备处理函数
	pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceCC;

	//当关闭文件句柄或断开与设备的连接时,通知驱动程序进行必要的清理工作的回调函数
	pDriverObject->MajorFunction[IRP_MJ_CLEANUP] = DeviceCC;

	// 设置用户驱动卸载处理函数
	pDriverObject->DriverUnload = DriverUnload;
	// 设置处理设备控制请求函数
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;
	/*
	详细解释：
		当用户请求打开设备时操作系统会生成IRP_MJ_CREATE请求，并传递给驱动程序处理
		驱动收到请求时会调用MajorFunction[IRP_MJ_CREATE] 来注册一个函数来处理请求
		会将设备设置为打开状态，
		这个函数示例中没有处理任何复杂逻辑(如检查权限，资源分配等).
	*/


	return status;
}
