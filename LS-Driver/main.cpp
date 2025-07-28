#include"driver.hpp"



/*
static 全局主要作用就是:
	1.存放在数据段(静态存储区)
	2.内部链接(无法使用extern也无法引用)
	3.生命周期为整个程序运行期间
static 局部主要作用就是:
	1.存放在数据段(静态存储区)
	2.内部链接(无法使用extern也无法引用)
	3.生命周期为整个程序运行期间
	4.仅函数调用初始化一次后续复用
*/

/*
特性				inline __forceinline										static	inline __forceinline
链接性				内部链接 (Internal Linkage)									外部链接 (External Linkage)
可见性				仅在当前编译单元 (.cpp 文件)内可见							在整个程序中可见
如何避免链接错误	每个文件生成一个私有副本，链接器互不干扰					inline 关键字告知链接器合并多个相同的定义
使用场景			C 语言风格，C/C++ 中都非常安全可靠	现代 C++ 风格，			用于在头文件中实现函数体
核心思想			“为我自己的文件创建一个本地副本”							“这是一个全局函数，但我允许它有多份相同的定义”

*/

NTSTATUS DriverEntry(
	PDRIVER_OBJECT  pDriverObject,/*加载驱动会实例化这个驱动对象：用来描述驱动结构*/
	PUNICODE_STRING pRegistryPath/*驱动注册表路径*/
)
{
	//这是一个宏，用于告诉编译器这个参数未被使用，以避免编译器发出未使用参数的警告
	UNREFERENCED_PARAMETER(pRegistryPath);

	DbgPrint("Driver:%wZ\n", pRegistryPath);
	NTSTATUS status;
	static UNICODE_STRING devName;//定义一个UNICODE_STRING结构体，用于存储设备名称。
	static UNICODE_STRING symLink;//创建符号链接，用户模式可以通过该链接访问设备
	// 创建设备对象
	RtlInitUnicodeString(&devName, L"\\Device\\liaoStarDriver");              //动态初始化设备对象
	RtlInitUnicodeString(&symLink, L"\\DosDevices\\liaoStarDriver");          //动态初始化符号链接


	PDEVICE_OBJECT pDeviceObject = NULL;//定义一个设备对象指针
	status = IoCreateDevice(
		pDriverObject,            // 驱动对象，可以尝试为其他驱动创建设备对象
		00,                      // 设备扩展大小(就是分配内存空间，0表示不申请内存，200表示分配200字节)
		&devName,               // 设备名称
		FILE_DEVICE_UNKNOWN,   // 设备类型
		0,                    // 设备特征
		TRUE,               // 设备是否独占
		&pDeviceObject      // 传出设备对象指针
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
