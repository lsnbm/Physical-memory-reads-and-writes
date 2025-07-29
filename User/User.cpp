#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <windows.h>
#include <winuser.h>   // For MapVirtualKeyA and GetAsyncKeyState
#include <tlhelp32.h>  // For CreateToolhelp32Snapshot
#include"User.hpp"
// 为C风格内存搜索功能添加必要的头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include"driver_data.h"

struct ProcessInfo {
	uint32_t pid;
	uint32_t parentPid;
};


uint32_t GetPidByName(const wchar_t* procName) {
	std::vector<ProcessInfo> matchingProcs;

	// 1. 获取所有进程的快照
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE) {
		return 0;
	}

	PROCESSENTRY32W pe;
	pe.dwSize = sizeof(pe);

	// 2. 遍历所有进程，找出所有匹配的进程并存储信息
	if (Process32FirstW(hSnap, &pe)) {
		do {
			if (_wcsicmp(pe.szExeFile, procName) == 0) {
				// 找到一个匹配项，将其PID和父进程PID存入列表
				matchingProcs.push_back({ pe.th32ProcessID, pe.th32ParentProcessID });
			}
		} while (Process32NextW(hSnap, &pe));
	}
	CloseHandle(hSnap);

	// 3. 分析结果
	if (matchingProcs.empty()) {
		// 没有找到任何匹配的进程
		return 0;
	}

	if (matchingProcs.size() == 1) {
		// 只找到一个，直接返回它的PID
		return matchingProcs[0].pid;
	}

	// 找到多个匹配的进程（典型的启动器+游戏场景）
	// 创建一个包含所有匹配进程PID的集合，用于快速查找
	std::unordered_set<uint32_t> pids;
	for (const auto& proc : matchingProcs) {
		pids.insert(proc.pid);
	}

	// 遍历所有匹配的进程，寻找那个父进程也在这个匹配列表中的进程
	// 这个进程就是我们寻找的子进程（游戏主进程）
	for (const auto& proc : matchingProcs) {
		// 如果一个进程的父进程PID存在于我们的PID集合中，
		// 那么这个进程(proc)就是子进程
		if (pids.count(proc.parentPid)) {
			return proc.pid;
		}
	}


	for (const auto& proc : matchingProcs) {
		bool isParent = false;
		for (const auto& otherProc : matchingProcs) {
			if (proc.pid == otherProc.parentPid) {
				isParent = true;
				break;
			}
		}
		if (!isParent) {
			return proc.pid;
		}
	}

	// 如果所有逻辑都失败，返回0表示无法明确识别
	return 0;
}



template<typename T>
bool SafeInput(const std::string& prompt, T& value, bool isHex = false) {
	std::cout << prompt;
	std::istream& stream = isHex ? std::cin >> std::hex : std::cin >> std::dec;
	stream >> value;

	if (std::cin.fail()) {
		std::cin.clear();
		std::cin.ignore(9999, '\n');
		std::cerr << "输入无效!" << std::endl;
		return false;
	}
	std::cin.ignore(9999, '\n');
	return true;
}


void HandleMemoryRead() {
	uintptr_t addr;
	if (!SafeInput("请输入要读取的地址 (十六进制): 0x", addr, true)) return;
	int typeChoice;
	if (!SafeInput("请选择数据类型: [1] int [2] long long [3] float [4] double: ", typeChoice)) return;

	std::cout << std::uppercase << std::hex;
	switch (typeChoice) {
	case 1: { auto val = AutoDriver::Read<int>(addr); std::cout << "值 (int): " << std::dec << val << " (0x" << val << ")" << std::endl; break; }
	case 2: { auto val = AutoDriver::Read<long long>(addr); std::cout << "值 (long long): " << std::dec << val << " (0x" << val << ")" << std::endl; break; }
	case 3: { auto val = AutoDriver::Read<float>(addr); std::cout << "值 (float): " << val << std::endl; break; }
	case 4: { auto val = AutoDriver::Read<double>(addr); std::cout << "值 (double): " << val << std::endl; break; }
	default: std::cerr << "无效的类型选择!" << std::endl;
	}
	std::cout << std::dec;
}

void HandleMemoryWrite() {
	uintptr_t addr;
	int value;
	if (SafeInput("请输入要写入的地址 (十六进制): 0x", addr, true) && SafeInput("请输入要写入的4字节整数值 (十进制): ", value)) {
		AutoDriver::Write<int>(addr, value) ? std::cout << "写入成功!" << std::endl : std::cerr << "写入失败!" << std::endl;
	}
}

void HandleProcessTermination() {
	uint32_t pid = AutoDriver::GetTargetProcessId();
	std::cout << "您确定要终止进程 " << pid << "? (y/n): ";
	char confirm;
	std::cin >> confirm;
	std::cin.ignore(9999, '\n');
	if (std::tolower(confirm) == 'y') {
		AutoDriver::TerminateTargetProcess(pid) ? std::cout << "进程已终止!" << std::endl : std::cerr << "终止失败!" << std::endl;
	}
}

void HandleGetModuleBase() {
	std::string moduleName;
	std::cout << "请输入模块名称 (留空则为主要可执行文件): ";
	std::getline(std::cin, moduleName);
	uintptr_t baseAddr = AutoDriver::GetModuleBase(moduleName);
	if (baseAddr) {
		std::cout << "基地址: 0x" << std::hex << std::uppercase << baseAddr << std::dec << std::endl;
	}
	else {
		std::cerr << "获取模块基地址失败!" << std::endl;
	}
}

void HandleKeyboardSim() {
	char key;
	std::cout << "请输入要模拟的按键 (例如: W): ";
	std::cin.get(key);
	std::cin.ignore(9999, '\n');
	USHORT scanCode = MapVirtualKeyA(toupper(key), MAPVK_VK_TO_VSC);
	if (scanCode == 0) {
		std::cerr << "无效的按键!" << std::endl;
		return;
	}
	std::cout << "3秒后将模拟按键，请切换到目标窗口..." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(3));

	// 直接调用驱动接口模拟按键的按下和弹起
	bool press = AutoDriver::SendKeyboardEvent(0, scanCode, KEY_MAKE, 0, 0);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	bool release = AutoDriver::SendKeyboardEvent(0, scanCode, KEY_BREAK, 0, 0);

	(press && release) ? std::cout << "按键模拟成功!" << std::endl : std::cerr << "按键模拟失败!" << std::endl;
}

void HandleMouseMove() {
	LONG dx, dy;
	if (SafeInput("请输入相对 X 轴移动量 (dx): ", dx) && SafeInput("请输入相对 Y 轴移动量 (dy): ", dy)) {
		// 直接调用驱动接口模拟鼠标相对移动
		bool success = AutoDriver::SendMouseEvent(0, MOUSE_MOVE_RELATIVE, 0, 0, 0, dx, dy, 0);
		success ? std::cout << "鼠标移动成功!" << std::endl : std::cerr << "鼠标移动失败!" << std::endl;
	}
}

void HandleMouseClick() {
	char clickType;
	std::cout << "请选择点击类型: [a] 左键单击 [b] 右键单击: ";
	std::cin.get(clickType);
	std::cin.ignore(9999, '\n');

	std::cout << "3秒后将模拟鼠标点击，请切换到目标窗口..." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(3));

	bool success = false;
	if (tolower(clickType) == 'a') {
		// 直接调用驱动接口模拟左键的按下和弹起
		bool down = AutoDriver::SendMouseEvent(0, 0, MOUSE_LEFT_BUTTON_DOWN, 0, 0, 0, 0, 0);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		bool up = AutoDriver::SendMouseEvent(0, 0, MOUSE_LEFT_BUTTON_UP, 0, 0, 0, 0, 0);
		success = down && up;
	}
	else if (tolower(clickType) == 'b') {
		// 直接调用驱动接口模拟右键的按下和弹起
		bool down = AutoDriver::SendMouseEvent(0, 0, MOUSE_RIGHT_BUTTON_DOWN, 0, 0, 0, 0, 0);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		bool up = AutoDriver::SendMouseEvent(0, 0, MOUSE_RIGHT_BUTTON_UP, 0, 0, 0, 0, 0);
		success = down && up;
	}
	else {
		std::cerr << "无效的选择!" << std::endl;
		return;
	}

	success ? std::cout << "鼠标点击成功!" << std::endl : std::cerr << "鼠标点击失败!" << std::endl;
}

void HandleMouseScroll() {
	SHORT scroll;
	if (SafeInput("请输入滚轮滚动量 (正数向上, 负数向下): ", scroll)) {
		// 直接调用驱动接口模拟滚轮滚动
		bool success = AutoDriver::SendMouseEvent(0, 0, MOUSE_WHEEL, static_cast<USHORT>(scroll), 0, 0, 0, 0);
		success ? std::cout << "鼠标滚轮滚动成功!" << std::endl : std::cerr << "鼠标滚轮滚动失败!" << std::endl;
	}
}

void HandleElevation() {
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, AutoDriver::GetTargetProcessId());
	if (hProcess == nullptr) {
		std::cerr << "OpenProcess 调用失败! 错误码: " << GetLastError() << std::endl;
		return;
	}
	std::cout << "成功获取到一个受限的句柄: 0x" << std::hex << (uintptr_t)hProcess << std::dec << std::endl;
	if (AutoDriver::HandleElevation(hProcess)) {
		std::cout << "驱动报告提权成功!" << std::endl;
	}
	else {
		std::cerr << "!!! 驱动报告提权失败 !!! 错误码: " << GetLastError() << std::endl;
	}
	CloseHandle(hProcess);
}

void RunAllPerformanceTests() {
	uint32_t selfPid = GetCurrentProcessId();
	std::cout << "\n正在对当前进程 (PID: " << selfPid << ") 运行性能测试...\n" << std::endl;
	AutoDriver::SetTargetProcessId(selfPid);
	srand(static_cast<unsigned int>(time(nullptr)));

	const auto run_test = [](const std::string& name, int iterations, const std::function<void()>& test_logic, const std::function<void()>& validation = nullptr) {
		std::cout << "--- 开始测试: " << name << " ---" << std::endl;
		LARGE_INTEGER freq, start, end;
		QueryPerformanceFrequency(&freq);

		QueryPerformanceCounter(&start);
		for (int i = 0; i < iterations; ++i) test_logic();
		QueryPerformanceCounter(&end);

		double ms = (end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
		double avg_us = ms * 1000.0 / iterations;

		std::cout << "总计耗时: " << std::fixed << std::setprecision(2) << ms << " 毫秒, 共 " << iterations << " 次迭代" << std::endl;
		std::cout << "平均延迟: " << std::fixed << std::setprecision(4) << avg_us << " 微秒/操作" << std::endl;
		if (validation) validation();
		std::cout << "--------------------------------------------------\n" << std::endl;
		};

	// --- 1. 32位地址测试 (分页与非分页) ---
	{
		const size_t allocSize = 4096 * 10;
		const int numIntegers = allocSize / sizeof(int);
		long long mismatchCount = 0;

		LPVOID ptr = VirtualAlloc(
			(LPVOID)0x20000000,
			allocSize,
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE
		);

		if (ptr == NULL) {
			std::cerr << "!!! 32位地址 VirtualAlloc 分配失败。错误代码: " << GetLastError() << std::endl;
			std::cerr << "!!! 无法执行所有32位地址相关的测试。\n" << std::endl;
		}
		else {
			std::cout << ">>> 开始32位地址测试, 成功分配内存, 基址: " << ptr << " <<<\n" << std::endl;
			int* testData = static_cast<int*>(ptr);
			for (int i = 0; i < numIntegers; ++i) testData[i] = i;

			// --- 1a. 32位地址分页内存读取测试 ---
			{
				mismatchCount = 0;
				auto logic = [&]() {
					int randomIndex = rand() % numIntegers;
					uintptr_t targetAddress = reinterpret_cast<uintptr_t>(testData + randomIndex);
					if (AutoDriver::Read<int>(targetAddress) != randomIndex) mismatchCount++;
					};
				auto validation = [&]() {
					if (mismatchCount > 0) std::cerr << "!!! 验证失败: " << mismatchCount << " 次数据不匹配!!!" << std::endl;
					else std::cout << "验证成功: 所有数据均被正确读取。" << std::endl;
					};
				run_test("32位地址 分页内存读取性能", 1000000, logic, validation);
			}

			// --- 1b. 32位地址非分页内存读取测试 ---
			{
				mismatchCount = 0;
				if (!VirtualLock(ptr, allocSize)) {
					std::cerr << "!!! VirtualLock 失败 (错误码: " << GetLastError() << ")。无法执行32位地址的非分页内存测试。" << std::endl;
					std::cerr << "!!! 这通常是因为程序缺少 'SeLockMemoryPrivilege' (锁定内存页) 权限。\n" << std::endl;
				}
				else {
					auto logic = [&]() {
						int randomIndex = rand() % numIntegers;
						uintptr_t targetAddress = reinterpret_cast<uintptr_t>(testData + randomIndex);
						if (AutoDriver::Read<int>(targetAddress) != randomIndex) mismatchCount++;
						};
					auto validation = [&]() {
						if (mismatchCount > 0) std::cerr << "!!! 验证失败: " << mismatchCount << " 次数据不匹配!!!" << std::endl;
						else std::cout << "验证成功: 所有数据均被正确读取。" << std::endl;
						};
					run_test("32位地址 非分页内存读取性能", 1000000, logic, validation);
					VirtualUnlock(ptr, allocSize);
				}
			}

			VirtualFree(ptr, 0, MEM_RELEASE);
		}
	}

	// --- 2. 64位地址测试 (分页与非分页) ---
	{
		const size_t allocSize = 4096 * 10;
		const int numIntegers = allocSize / sizeof(int);
		long long mismatchCount = 0;

		LPVOID ptr = VirtualAlloc(
			NULL,
			allocSize,
			MEM_COMMIT | MEM_RESERVE,
			PAGE_READWRITE
		);

		if (ptr == NULL) {
			std::cerr << "!!! 64位地址 VirtualAlloc 分配失败。错误代码: " << GetLastError() << std::endl;
			std::cerr << "!!! 无法执行所有64位地址相关的测试。\n" << std::endl;
		}
		else {
			std::cout << ">>> 开始64位地址测试, 成功分配内存, 基址: " << ptr << " <<<\n" << std::endl;
			int* testData = static_cast<int*>(ptr);
			for (int i = 0; i < numIntegers; ++i) testData[i] = i;

			// --- 2a. 64位地址分页内存读取测试 ---
			{
				mismatchCount = 0;
				auto logic = [&]() {
					int randomIndex = rand() % numIntegers;
					uintptr_t targetAddress = reinterpret_cast<uintptr_t>(testData + randomIndex);
					if (AutoDriver::Read<int>(targetAddress) != randomIndex) mismatchCount++;
					};
				auto validation = [&]() {
					if (mismatchCount > 0) std::cerr << "!!! 验证失败: " << mismatchCount << " 次数据不匹配!!!" << std::endl;
					else std::cout << "验证成功: 所有数据均被正确读取。" << std::endl;
					};
				run_test("64位地址 分页内存读取性能", 1000000, logic, validation);
			}

			// --- 2b. 64位地址非分页内存读取测试 ---
			{
				mismatchCount = 0;
				if (!VirtualLock(ptr, allocSize)) {
					std::cerr << "!!! VirtualLock 失败 (错误码: " << GetLastError() << ")。无法执行64位地址的非分页内存测试。" << std::endl;
					std::cerr << "!!! 这通常是因为程序缺少 'SeLockMemoryPrivilege' (锁定内存页) 权限。\n" << std::endl;
				}
				else {
					auto logic = [&]() {
						int randomIndex = rand() % numIntegers;
						uintptr_t targetAddress = reinterpret_cast<uintptr_t>(testData + randomIndex);
						if (AutoDriver::Read<int>(targetAddress) != randomIndex) mismatchCount++;
						};
					auto validation = [&]() {
						if (mismatchCount > 0) std::cerr << "!!! 验证失败: " << mismatchCount << " 次数据不匹配!!!" << std::endl;
						else std::cout << "验证成功: 所有数据均被正确读取。" << std::endl;
						};
					run_test("64位地址 非分页内存读取性能", 1000000, logic, validation);
					VirtualUnlock(ptr, allocSize);
				}
			}

			VirtualFree(ptr, 0, MEM_RELEASE);
		}
	}

	// --- 3. 空 IO 调用测试 ---
	{
		run_test("空 IO 调用性能", 1000000, [selfPid]() {
			AutoDriver::SetTargetProcessId(selfPid);
			});
	}

	std::cout << "\n所有性能测试完成。" << std::endl;
	system("pause");
}



void HandleMemorySearch() {
	// --- 内部状态管理 ---
	std::vector<uintptr_t> results;
	bool is_first_scan = true;

	// --- 准备工作：获取进程句柄 ---
	printf("正在为内存扫描准备句柄...\n_DE_OPERATION 用于 VirtualQueryEx");
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_OPERATION, FALSE, AutoDriver::GetTargetProcessId());
	if (hProcess == NULL) {
		fprintf(stderr, "错误: OpenProcess 调用失败，无法获取进程句柄。请检查权限。\n");
		fprintf(stderr, "错误码: %lu\n", GetLastError());
		return;
	}
	printf("成功获取临时句柄: 0x%p\n", hProcess);

	// --- 选择数据类型 ---
	int type_choice = 0;
	size_t value_size = 0;
	while (true) {
		printf("\n请选择要搜索的数据类型:\n");
		printf(" [1] 整数 (int, 4字节)\n");
		printf(" [2] 浮点数 (float, 4字节)\n");
		printf(" [3] 双精度浮点数 (double, 8字节)\n");
		printf(" [0] 返回\n");
		printf("选择: ");
		if (scanf("%d", &type_choice) != 1) {
			while (getchar() != '\n');
			fprintf(stderr, "无效的选择，请输入数字。\n");
			continue;
		}
		while (getchar() != '\n');
		if (type_choice == 1) { value_size = sizeof(int); break; }
		if (type_choice == 2) { value_size = sizeof(float); break; }
		if (type_choice == 3) { value_size = sizeof(double); break; }
		if (type_choice == 0) {
			CloseHandle(hProcess);
			printf("句柄已关闭。\n");
			return;
		}
		fprintf(stderr, "无效的选择，请重试。\n");
	}

	// --- 主搜索循环 ---
	while (true) {
		printf("\n输入要搜索的值 (或输入 'n' 开始新搜索, 'q' 退出): ");
		char input_buffer[100];
		if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) continue;
		input_buffer[strcspn(input_buffer, "\r\n")] = 0;

		if (_stricmp(input_buffer, "q") == 0) break;
		if (_stricmp(input_buffer, "n") == 0) {
			results.clear();
			is_first_scan = true;
			printf("内存搜索器已重置。\n");
			continue;
		}

		char value_storage[8];
		void* pValue = value_storage;
		switch (type_choice) {
		case 1: *(int*)pValue = atoi(input_buffer); break;
		case 2: *(float*)pValue = (float)atof(input_buffer); break;
		case 3: *(double*)pValue = atof(input_buffer); break;
		}

		if (is_first_scan) {
			// --- 首次扫描逻辑 ---
			printf("正在开始首次扫描... 这可能需要一些时间。\n");
			auto startTime = std::chrono::high_resolution_clock::now();
			size_t total_memory_scanned = 0;
			const size_t CHUNK_SIZE = 65536; // 64KB
			char* buffer = (char*)malloc(CHUNK_SIZE);
			if (!buffer) {
				fprintf(stderr, "错误: 无法为扫描缓冲区分配内存!\n");
				break;
			}

			uintptr_t current_address = 0;
			MEMORY_BASIC_INFORMATION mbi;
			MEMORY_BASIC_INFORMATION mbi_check; // 用于调用驱动前的最后检查

			// 1. 宏观扫描：找到大的有效内存区域
			while (VirtualQueryEx(hProcess, (LPCVOID)current_address, &mbi, sizeof(mbi))) {
				if (mbi.State == MEM_COMMIT) { // 只关心已提交的内存
					uintptr_t chunk_base = (uintptr_t)mbi.BaseAddress;
					size_t bytes_left_in_region = mbi.RegionSize;

					// 2. 微观扫描：将大区域分解成小块
					while (bytes_left_in_region > 0) {
						size_t bytes_to_read = min(bytes_left_in_region, CHUNK_SIZE);

						// 3. 【最关键的修复】在调用驱动前，对当前小块进行“最后一刻”的有效性检查
						if (VirtualQueryEx(hProcess, (LPCVOID)chunk_base, &mbi_check, sizeof(mbi_check)) &&
							mbi_check.State == MEM_COMMIT && // 必须仍然是已提交
							(mbi_check.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) && // 必须可读
							!(mbi_check.Protect & PAGE_NOACCESS)) // 必须不是“无访问权限”
						{
							// 4. 只有检查通过，才调用驱动
							if (AutoDriver::Read(chunk_base, buffer, bytes_to_read)) {
								total_memory_scanned += bytes_to_read;
								for (size_t i = 0; i <= bytes_to_read - value_size; ++i) {
									if (memcmp(buffer + i, pValue, value_size) == 0) {
										results.push_back(chunk_base + i);
									}
								}
							}
						}

						bytes_left_in_region -= bytes_to_read;
						chunk_base += bytes_to_read;
					}
				}
				current_address = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
			}

			free(buffer);
			auto endTime = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> elapsed = endTime - startTime;
			printf("首次扫描完成! 耗时: %.2f 秒, 扫描总内存: %llu MB。\n", elapsed.count(), total_memory_scanned / (1024 * 1024));
			is_first_scan = false;
		}
		else {
			// --- 再次扫描 (过滤) 逻辑 ---
			if (results.empty()) {
				printf("没有可供筛选的地址。请先进行首次扫描。\n");
				continue;
			}
			printf("正在筛选 %zu 个地址...\n", results.size());
			std::vector<uintptr_t> next_results;
			char value_buffer[8];
			MEMORY_BASIC_INFORMATION mbi_check; // 用于重新检查内存状态

			for (const auto& addr : results) {
				// 【最关键的修复】在读取每个旧地址前，都进行一次“最后一刻”的有效性检查
				if (VirtualQueryEx(hProcess, (LPCVOID)addr, &mbi_check, sizeof(mbi_check)) &&
					mbi_check.State == MEM_COMMIT &&
					(mbi_check.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) &&
					!(mbi_check.Protect & PAGE_NOACCESS))
				{
					// 只有检查通过，才调用驱动
					if (AutoDriver::Read(addr, value_buffer, value_size)) {
						if (memcmp(value_buffer, pValue, value_size) == 0) {
							next_results.push_back(addr);
						}
					}
				}
			}
			results = next_results;
		}

		// --- 统一的结果显示逻辑 ---
		printf("找到 %zu 个结果。\n", results.size());
		if (!results.empty()) {
			size_t display_count = (results.size() > 30) ? 30 : results.size();
			printf("--- 显示前 %zu 个搜索结果 ---\n", display_count);
			for (size_t i = 0; i < display_count; ++i) {
				uintptr_t current_addr = results[i];
				printf("地址: 0x%llX", (unsigned long long)current_addr);
				switch (type_choice) {
				case 1: printf(" | 当前值 (int): %d\n", AutoDriver::Read<int>(current_addr)); break;
				case 2: printf(" | 当前值 (float): %f\n", AutoDriver::Read<float>(current_addr)); break;
				case 3: printf(" | 当前值 (double): %lf\n", AutoDriver::Read<double>(current_addr)); break;
				}
			}
			if (results.size() > 30) {
				printf("... (还有 %zu 个结果未显示)\n", results.size() - 30);
			}
			printf("---------------------------\n");
		}
	}

	// --- 清理工作 ---
	CloseHandle(hProcess);
	printf("句柄已关闭，退出内存搜索。\n");
}


void RunFunctionalTests() {
	uint32_t targetPid = 0;
	if (!SafeInput("请输入目标进程 PID: ", targetPid)) return;
	AutoDriver::SetTargetProcessId(targetPid);

	const std::map<int, std::pair<std::string, std::function<void()>>> menuActions = {
		{1, {"读取内存",         HandleMemoryRead}},
		{2, {"写入内存",         HandleMemoryWrite}},
		{3, {"终止进程",         HandleProcessTermination}},
		{4, {"获取模块基址",     HandleGetModuleBase}},
		{5, {"模拟键盘",         HandleKeyboardSim}},
		{6, {"模拟鼠标移动",     HandleMouseMove}},
		{7, {"模拟鼠标点击",     HandleMouseClick}},
		{8, {"模拟鼠标滚轮",     HandleMouseScroll}},
		{9, {"句柄提权",         HandleElevation}},
		{10,{"内存搜索",         HandleMemorySearch}}
	};

	bool running = true;
	while (running) {
		system("cls");
		std::cout << "================== 驱动工具菜单 ==================\n"
			<< " 目标进程 PID: " << AutoDriver::GetTargetProcessId() << "\n\n";
		for (const auto& item : menuActions) {
			std::cout << " [" << std::setw(2) << item.first << "] " << item.second.first << std::endl;
		}
		std::cout << " [0] 返回主菜单\n"
			<< "======================================================\n"
			<< "请选择一个选项: ";

		int choice;
		if (!(std::cin >> choice)) {
			std::cin.clear(); std::cin.ignore(9999, '\n');
			std::cerr << "输入无效，请输入一个数字。" << std::endl;
			system("pause"); continue;
		}
		std::cin.ignore(9999, '\n');

		if (choice == 0) {
			running = false;
			continue;
		}

		auto it = menuActions.find(choice);
		if (it != menuActions.end()) {
			it->second.second();
		}
		else {
			std::cerr << "无效的选择!" << std::endl;
		}

		std::cout << "\n按任意键继续...";
		system("pause > nul");
	}
	std::cout << "\n正在返回主菜单。" << std::endl;
	system("pause");
}


void RunLiveInputTest() {
	uint32_t targetPid = 0;
	if (!SafeInput("请输入用于实时输入测试的目标进程 PID: ", targetPid)) return;
	AutoDriver::SetTargetProcessId(targetPid);

	std::cout << "\n--- 实时输入测试已激活 ---" << std::endl
		<< "  [左方向键]  - 模拟左键单击" << std::endl
		<< "  [右方向键]  - 模拟右键单击" << std::endl
		<< "  [上方向键]  - 模拟滚轮向上" << std::endl
		<< "  [下方向键]  - 模拟滚轮向下" << std::endl
		<< "  [K 键]        - 模拟空格键按下" << std::endl
		<< "  [Y 键]        - 模拟鼠标拖拽 (右下方向)" << std::endl
		<< "  [Q 键]        - 退出实时测试" << std::endl
		<< "------------------------------" << std::endl;

	while (!(GetAsyncKeyState('Q') & 0x8000)) {
		if (GetAsyncKeyState(VK_LEFT) & 0x8000) {
			AutoDriver::SendMouseEvent(0, 0, MOUSE_LEFT_BUTTON_DOWN, 0, 0, 0, 0, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			AutoDriver::SendMouseEvent(0, 0, MOUSE_LEFT_BUTTON_UP, 0, 0, 0, 0, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
			AutoDriver::SendMouseEvent(0, 0, MOUSE_RIGHT_BUTTON_DOWN, 0, 0, 0, 0, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			AutoDriver::SendMouseEvent(0, 0, MOUSE_RIGHT_BUTTON_UP, 0, 0, 0, 0, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		if (GetAsyncKeyState(VK_UP) & 0x8000) {
			AutoDriver::SendMouseEvent(0, 0, MOUSE_WHEEL, 120, 0, 0, 0, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
			AutoDriver::SendMouseEvent(0, 0, MOUSE_WHEEL, static_cast<USHORT>(-120), 0, 0, 0, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		if (GetAsyncKeyState('K') & 0x8000) {
			USHORT scanCode = MapVirtualKeyA(VK_SPACE, MAPVK_VK_TO_VSC);
			AutoDriver::SendKeyboardEvent(0, scanCode, KEY_MAKE, 0, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			AutoDriver::SendKeyboardEvent(0, scanCode, KEY_BREAK, 0, 0);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		if (GetAsyncKeyState('Y') & 0x0001) {
			AutoDriver::SendMouseEvent(0, 0, MOUSE_LEFT_BUTTON_DOWN, 0, 0, 0, 0, 0);
			for (int i = 0; i < 100; i++) {
				AutoDriver::SendMouseEvent(0, MOUSE_MOVE_RELATIVE, 0, 0, 0, 1, 1, 0);
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
			AutoDriver::SendMouseEvent(0, 0, MOUSE_LEFT_BUTTON_UP, 0, 0, 0, 0, 0);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	std::cout << "\n实时输入测试结束，正在返回主菜单。" << std::endl;
	system("pause");
}

void ShowMainMenu() {
	system("cls");
	std::cout << "==================== 主菜单 =====================" << std::endl
		<< " [1] 功能测试 (交互式菜单)" << std::endl
		<< " [2] 性能测试 (自动化基准)" << std::endl
		<< " [3] 实时输入测试 (实时按键模拟)" << std::endl
		<< " [0] 退出程序" << std::endl
		<< "====================================================" << std::endl
		<< "请选择一个选项: ";
}

int main() {


	AutoDriver driver(L"\\\\.\\liaoStarDriver", Driver_sys, sizeof(Driver_sys));
	if (!AutoDriver::IsInitialized()) {
		std::cerr << "[!] 严重错误: 驱动初始化失败。程序即将退出。" << std::endl;
		system("pause");
		return -1;
	}

	const std::map<int, std::function<void()>> mainActions = {
		{1, RunFunctionalTests},
		{2, RunAllPerformanceTests},
		{3, RunLiveInputTest}
	};

	bool running = true;
	while (running) {
		ShowMainMenu();
		int choice;
		if (!(std::cin >> choice)) {
			std::cin.clear(); std::cin.ignore(9999, '\n');
			std::cerr << "输入无效，请重试。" << std::endl;
			system("pause"); continue;
		}
		std::cin.ignore(9999, '\n');

		if (choice == 0) {
			running = false;
			continue;
		}

		auto it = mainActions.find(choice);
		if (it != mainActions.end()) {
			it->second();
		}
		else {
			std::cout << "无效的选择，请重试。" << std::endl;
			system("pause");
		}
	}

	std::cout << "正在退出程序。驱动程序将会被自动卸载。" << std::endl;
	return 0;



	//printf("PID=%d\n", GetPidByName(L"TslGame.exe"));
	//system("pause");
}
