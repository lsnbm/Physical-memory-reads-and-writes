#pragma once
#include <ntifs.h>
#include <intrin.h>

// --- 结构体定义 ---

// 用于最终物理内存操作的中转页信息
struct PHYSICAL_PAGE_INFO
{
    PVOID BaseAddress;
    SIZE_T Size;
    PVOID PteAddress;
};

// 终极隐蔽方案的上下文结构体：缓存物理页表项的值
struct STEALTH_RW_CONTEXT
{
    ULONG64 TargetCr3;
    ULONG64 CachedPxeVal;
    ULONG64 CachedPxeVaRange;
    ULONG64 CachedPpeVal;
    ULONG64 CachedPpeVaRange;
    ULONG64 CachedPdeVal;
    ULONG64 CachedPdeVaRange;
};


// --- 全局变量 ---
static ULONG64 g_PteBaseForAlloc = 0;
static BOOLEAN g_IsInitPteBaseForAlloc = false;
static PPHYSICAL_MEMORY_RANGE g_PhysicalMemoryRanges = 0;


// --- 核心辅助函数与资源管理 ---

__forceinline KIRQL __fastcall RaiseIRQL(KIRQL NewIrql)
{
    KIRQL CurrentIrql = KeGetCurrentIrql();
    __writecr8(NewIrql);
    return CurrentIrql;
}

__forceinline KIRQL __fastcall RaiseIrqlToDpcLv()
{
    return RaiseIRQL(DISPATCH_LEVEL);
}

__forceinline void __fastcall LowerIrql(KIRQL Irql)
{
    __writecr8(Irql);
}

__forceinline bool __fastcall IsVaPhysicalAddressValid(PVOID VirtualAddress)
{
    return MmGetPhysicalAddress(VirtualAddress).QuadPart > PAGE_SIZE;
}

__forceinline PVOID __fastcall GetPML4Base(PHYSICAL_ADDRESS DirectoryTableBase)
{
    PVOID VirtualForPhysical = MmGetVirtualForPhysical(DirectoryTableBase);
    return ((ULONG64)VirtualForPhysical > PAGE_SIZE) ? VirtualForPhysical : NULL;
}

__forceinline bool __fastcall IsPhysPageInRange(ULONG64 PhysAddress, ULONG64 Size)
{
    const ULONG64 PhysPageEnd = PhysAddress + Size - 1;
    if (!g_PhysicalMemoryRanges)
    {
        if (KeGetCurrentIrql() != PASSIVE_LEVEL) return false;
        g_PhysicalMemoryRanges = MmGetPhysicalMemoryRanges();
    }
    if (!g_PhysicalMemoryRanges) return false;
    for (int i = 0; ; ++i)
    {
        const PHYSICAL_MEMORY_RANGE* range = &g_PhysicalMemoryRanges[i];
        if (!range->BaseAddress.QuadPart || !range->NumberOfBytes.QuadPart) break;
        if (PhysAddress >= range->BaseAddress.QuadPart && PhysPageEnd < (range->BaseAddress.QuadPart + range->NumberOfBytes.QuadPart))
        {
            return true;
        }
    }
    return false;
}

__forceinline ULONG __fastcall InitializePteBaseForAlloc()
{
    if (g_IsInitPteBaseForAlloc) return 0;
    PHYSICAL_ADDRESS DirectoryTableBase;
    ULONG64 Cr3 = __readcr3();
    DirectoryTableBase.QuadPart = (Cr3 & ~0xFFF);
    PULONG64 PML4Table = (PULONG64)GetPML4Base(DirectoryTableBase);
    if (!PML4Table) return 0x106;
    for (ULONG64 index = 0; index < 512; ++index)
    {
        if ((PML4Table[index] & 1) && (((PML4Table[index] >> 12) & 0xFFFFFFFFFF) == (Cr3 >> 12)))
        {
            g_PteBaseForAlloc = (index << 39) - 0x1000000000000;
            g_IsInitPteBaseForAlloc = TRUE;
            return 0;
        }
    }
    return 266;
}

__forceinline ULONG64 __fastcall GetPteAddressForAlloc(PVOID VirtualAddress)
{
    return g_PteBaseForAlloc + 8 * ((reinterpret_cast<ULONG64>(VirtualAddress) & 0xFFFFFFFFFFFFi64) >> 12);
}


//中转页，由外部调用
__forceinline ULONG __fastcall AllocatePhysicalPage(PHYSICAL_PAGE_INFO* PhysicalPageInfo)
{
    if (!PhysicalPageInfo) return 22;
    memset(PhysicalPageInfo, 0, sizeof(PHYSICAL_PAGE_INFO));
    const ULONG ErrorCode = InitializePteBaseForAlloc();
    if (ErrorCode) return ErrorCode;
    PVOID BaseAddress = MmAllocateMappingAddress(PAGE_SIZE, 'axe');
    if (!BaseAddress) return 0x119;
    PVOID PteAddress = (PVOID)GetPteAddressForAlloc(BaseAddress);
    if (!PteAddress || !IsVaPhysicalAddressValid(PteAddress))
    {
        MmFreeMappingAddress(BaseAddress, 'axe');
        return 0x109;
    }
    PhysicalPageInfo->BaseAddress = BaseAddress;
    PhysicalPageInfo->Size = PAGE_SIZE;
    PhysicalPageInfo->PteAddress = PteAddress;
    return 0;
}
__forceinline void __fastcall FreePhysicalPage(PHYSICAL_PAGE_INFO* PageInfo)
{
    if (PageInfo && PageInfo->BaseAddress)
    {
        MmFreeMappingAddress(PageInfo->BaseAddress, 'axe');
        memset(PageInfo, 0, sizeof(PHYSICAL_PAGE_INFO));
    }
}


//把目标物理地址映射到中转页，并进行读写操作中转
__forceinline ULONG __fastcall ReadPhysicalSinglePage(const PHYSICAL_PAGE_INFO* TransferPageInfo, ULONG64 PhysAddress, PVOID Buffer, SIZE_T Size)
{
    if (!PhysAddress || !Buffer || !Size) return 22;
    if (!TransferPageInfo || !TransferPageInfo->BaseAddress || !TransferPageInfo->PteAddress) return 157;
    if (Size > TransferPageInfo->Size) return 279;
    if ((PhysAddress >> 12) != ((PhysAddress + Size - 1) >> 12)) return 275;
    if (!IsPhysPageInRange(PhysAddress, Size)) return 276;

    KIRQL OldIrql;
    const BOOLEAN bRaisedIrql = (KeGetCurrentIrql() < DISPATCH_LEVEL);
    if (bRaisedIrql) OldIrql = RaiseIrqlToDpcLv();

    const PVOID pteAddress = TransferPageInfo->PteAddress;
    const PVOID transferBase = TransferPageInfo->BaseAddress;
    const ULONG64 oldPte = *(ULONG64*)pteAddress;

    *(ULONG64*)pteAddress = (PhysAddress & ~0xFFFULL) | (oldPte & 0xFFF0000000000FFF) | 0x103;
    __invlpg(transferBase);
    memmove(Buffer, (char*)transferBase + (PhysAddress & 0xFFF), Size);
    *(ULONG64*)pteAddress = oldPte;
    __invlpg(transferBase);

    if (bRaisedIrql) LowerIrql(OldIrql);
    return 0;
}

__forceinline ULONG __fastcall WritePhysicalSinglePage(const PHYSICAL_PAGE_INFO* TransferPageInfo, ULONG64 PhysAddress, PVOID Buffer, SIZE_T Size)
{
    if (!PhysAddress || !Buffer || !Size) return 22;
    if (!TransferPageInfo || !TransferPageInfo->BaseAddress || !TransferPageInfo->PteAddress) return 157;
    if (Size > TransferPageInfo->Size) return 279;
    if ((PhysAddress >> 12) != ((PhysAddress + Size - 1) >> 12)) return 275;
    if (!IsPhysPageInRange(PhysAddress, Size)) return 276;

    KIRQL OldIrql;
    const BOOLEAN bRaisedIrql = (KeGetCurrentIrql() < DISPATCH_LEVEL);
    if (bRaisedIrql) OldIrql = RaiseIrqlToDpcLv();

    const PVOID pteAddress = TransferPageInfo->PteAddress;
    const PVOID transferBase = TransferPageInfo->BaseAddress;
    const ULONG64 oldPte = *(ULONG64*)pteAddress;

    *(ULONG64*)pteAddress = (PhysAddress & ~0xFFFULL) | (oldPte & 0xFFF0000000000FFF) | 0x103;
    __invlpg(transferBase);
    memmove((char*)transferBase + (PhysAddress & 0xFFF), Buffer, Size);
    *(ULONG64*)pteAddress = oldPte;
    __invlpg(transferBase);

    if (bRaisedIrql) LowerIrql(OldIrql);
    return 0;
}


//最重要的缓存机制
__forceinline ULONG GetPhysPageInfoStealth(const PHYSICAL_PAGE_INFO* TransferPageInfo,ULONG64 Cr3,PVOID Va,PULONG64 pPhysicalPageBase,PULONG64 pPageSize,STEALTH_RW_CONTEXT* Context)
{
    if (!pPhysicalPageBase || !pPageSize || !Context) return 22;

    const ULONG64 va_val = (ULONG64)Va;
    const ULONG64 pxe_va_range = va_val >> 39;
    const ULONG64 ppe_va_range = va_val >> 30;
    const ULONG64 pde_va_range = va_val >> 21;

    ULONG64 pxe_val, ppe_val, pde_val, pte_val;

    // 1. 检查CR3，如果变化则清空所有缓存
    if (Context->TargetCr3 != Cr3)
    {
        memset(Context, 0, sizeof(STEALTH_RW_CONTEXT));
        Context->TargetCr3 = Cr3;
    }

    // 2. PXE (PML4E) 级处理
    if (Context->CachedPxeVaRange != pxe_va_range)
    {
        // 只要PXE变了，下面的一切都必须重新计算
        if (ReadPhysicalSinglePage(TransferPageInfo, (Cr3 & ~0xFFF) + 8 * (pxe_va_range & 0x1FF), &pxe_val, 8) || (pxe_val & 1) == 0) return 262;
        Context->CachedPxeVal = pxe_val;
        Context->CachedPxeVaRange = pxe_va_range;
        Context->CachedPpeVaRange = (ULONG64)-1; // 强制下级失效
        Context->CachedPdeVaRange = (ULONG64)-1;
    }
    pxe_val = Context->CachedPxeVal;

    // 3. PPE (PDPTE) 级处理
    if (Context->CachedPpeVaRange != ppe_va_range)
    {
        const ULONG64 ppe_phys_addr = (pxe_val & 0x000FFFFFFFFF000) + 8 * (ppe_va_range & 0x1FF);
        if (ReadPhysicalSinglePage(TransferPageInfo, ppe_phys_addr, &ppe_val, 8) || (ppe_val & 1) == 0) return 263;
        Context->CachedPpeVal = ppe_val;
        Context->CachedPpeVaRange = ppe_va_range;
        Context->CachedPdeVaRange = (ULONG64)-1; // 强制下级失效
    }
    ppe_val = Context->CachedPpeVal;
    if (ppe_val & 0x80) { *pPageSize = 0x40000000; *pPhysicalPageBase = (ppe_val & 0x000FFFFFC0000000); return 0; }

    // 4. PDE 级处理
    if (Context->CachedPdeVaRange != pde_va_range)
    {
        const ULONG64 pde_phys_addr = (ppe_val & 0x000FFFFFFFFF000) + 8 * (pde_va_range & 0x1FF);
        if (ReadPhysicalSinglePage(TransferPageInfo, pde_phys_addr, &pde_val, 8) || (pde_val & 1) == 0) return 264;
        Context->CachedPdeVal = pde_val;
        Context->CachedPdeVaRange = pde_va_range;
    }
    pde_val = Context->CachedPdeVal;
    if (pde_val & 0x80) { *pPageSize = 0x200000; *pPhysicalPageBase = (pde_val & 0x000FFFFFFE00000); return 0; }

    // 5. PTE 级（无缓存，总是直接读取）
    const ULONG64 pte_phys_addr = (pde_val & 0x000FFFFFFFFF000) + 8 * ((va_val >> 12) & 0x1FF);
    if (ReadPhysicalSinglePage(TransferPageInfo, pte_phys_addr, &pte_val, 8) || (pte_val & 1) == 0) return 265;

    *pPageSize = 0x1000;
    *pPhysicalPageBase = (pte_val & 0x000FFFFFFFFF000);
    return 0;
}

//最重要的缓存机制，调试函数
__forceinline ULONG GetPhysPageInfoStealth调试输出(const PHYSICAL_PAGE_INFO* TransferPageInfo,ULONG64 Cr3,PVOID Va,PULONG64 pPhysicalPageBase,PULONG64 pPageSize,STEALTH_RW_CONTEXT* Context)
{
    if (!pPhysicalPageBase || !pPageSize || !Context) return 22;

    const ULONG64 va_val = (ULONG64)Va;
    const ULONG64 pxe_va_range = va_val >> 39;
    const ULONG64 ppe_va_range = va_val >> 30;
    const ULONG64 pde_va_range = va_val >> 21;

    ULONG64 pxe_val, ppe_val, pde_val, pte_val;

    // --- 调试打印: 函数入口 ---
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "[MyDriver] GetPhys: VA=%p, CR3=%llX. Cache Check Start...\n", Va, Cr3);

    // 1. 检查CR3，如果变化则清空所有缓存
    if (Context->TargetCr3 != Cr3)
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: CR3 changed! Invalidating all cache.\n");
        memset(Context, 0, sizeof(STEALTH_RW_CONTEXT));
        Context->TargetCr3 = Cr3;
    }

    // 2. PXE (PML4E) 级缓存检查
    if (Context->CachedPxeVaRange == pxe_va_range)
    {
        pxe_val = Context->CachedPxeVal; // 命中缓存
        // --- 调试打印: PXE 命中 ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PXE Cache HIT for range %llX.\n", pxe_va_range);
    }
    else
    {
        // --- 调试打印: PXE 未命中 ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PXE Cache MISS for range %llX. Reading physical...\n", pxe_va_range);

        // 未命中，从CR3物理读取
        if (ReadPhysicalSinglePage(TransferPageInfo, (Cr3 & ~0xFFF) + 8 * (pxe_va_range & 0x1FF), &pxe_val, 8) || (pxe_val & 1) == 0) return 262;

        // 更新缓存
        Context->CachedPxeVal = pxe_val;
        Context->CachedPxeVaRange = pxe_va_range;
        Context->CachedPpeVaRange = (ULONG64)-1; // 下级缓存失效
        Context->CachedPdeVaRange = (ULONG64)-1;
    }

    // 3. PPE (PDPTE) 级缓存检查
    if (Context->CachedPpeVaRange == ppe_va_range)
    {
        ppe_val = Context->CachedPpeVal; // 命中缓存
        // --- 调试打印: PPE 命中 ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PPE Cache HIT for range %llX.\n", ppe_va_range);
    }
    else
    {
        // --- 调试打印: PPE 未命中 ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PPE Cache MISS for range %llX. Reading physical...\n", ppe_va_range);

        // 未命中，从PXE指向的物理页读取
        const ULONG64 ppe_phys_addr = (pxe_val & 0x000FFFFFFFFF000) + 8 * (ppe_va_range & 0x1FF);
        if (ReadPhysicalSinglePage(TransferPageInfo, ppe_phys_addr, &ppe_val, 8) || (ppe_val & 1) == 0) return 263;

        // 更新缓存
        Context->CachedPpeVal = ppe_val;
        Context->CachedPpeVaRange = ppe_va_range;
        Context->CachedPdeVaRange = (ULONG64)-1; // 下级缓存失效
    }
    if (ppe_val & 0x80) { *pPageSize = 0x40000000; *pPhysicalPageBase = (ppe_val & 0x000FFFFFC0000000); return 0; }

    // 4. PDE 级缓存检查
    if (Context->CachedPdeVaRange == pde_va_range)
    {
        pde_val = Context->CachedPdeVal; // 命中缓存
        // --- 调试打印: PDE 命中 ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PDE Cache HIT for range %llX.\n", pde_va_range);
    }
    else
    {
        // --- 调试打印: PDE 未命中 ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PDE Cache MISS for range %llX. Reading physical...\n", pde_va_range);

        // 未命中，从PPE指向的物理页读取
        const ULONG64 pde_phys_addr = (ppe_val & 0x000FFFFFFFFF000) + 8 * (pde_va_range & 0x1FF);
        if (ReadPhysicalSinglePage(TransferPageInfo, pde_phys_addr, &pde_val, 8) || (pde_val & 1) == 0) return 264;

        // 更新缓存
        Context->CachedPdeVal = pde_val;
        Context->CachedPdeVaRange = pde_va_range;
    }
    if (pde_val & 0x80) { *pPageSize = 0x200000; *pPhysicalPageBase = (pde_val & 0x000FFFFFFE00000); return 0; }

    // 5. PTE 级（无缓存，总是直接读取）
    const ULONG64 pte_phys_addr = (pde_val & 0x000FFFFFFFFF000) + 8 * ((va_val >> 12) & 0x1FF);
    if (ReadPhysicalSinglePage(TransferPageInfo, pte_phys_addr, &pte_val, 8) || (pte_val & 1) == 0) return 265;

    *pPageSize = 0x1000;
    *pPhysicalPageBase = (pte_val & 0x000FFFFFFFFF000);
    return 0;
}

// --- 最终的、统一的、智能的读写接口 ---

__forceinline ULONG ReadVirtualMemory(
    const PHYSICAL_PAGE_INFO* TransferPageInfo,
    ULONG64 DirectoryTableBase,
    PVOID Address,
    PVOID Buffer,
    ULONG TotalSize,
    STEALTH_RW_CONTEXT* Context)
{
    if (!Address || !Buffer || !TotalSize || !TransferPageInfo || !Context || !DirectoryTableBase) return 22;
    if (KeGetCurrentIrql() > DISPATCH_LEVEL) return 261;

    // 1. 初始探测
    ULONG64 page_phys_base = 0;
    ULONG64 page_size = 0;
    ULONG status = GetPhysPageInfoStealth(TransferPageInfo, DirectoryTableBase, Address, &page_phys_base, &page_size, Context);
    if (status != 0) return status;

    // 2. 智能判断
    const ULONG64 offset_in_page = (ULONG64)Address & (page_size - 1);
    if ((offset_in_page + TotalSize) <= page_size)
    {
        // 3. 快速路径：整个读取操作都在一个物理页内
        const ULONG64 start_phys_addr = page_phys_base + offset_in_page;
        ULONG bytes_remaining = TotalSize;
        ULONG64 current_phys_addr = start_phys_addr;
        PUCHAR current_buffer = (PUCHAR)Buffer;

        while (bytes_remaining > 0)
        {
            ULONG chunk_size = min(bytes_remaining, PAGE_SIZE - (ULONG)(current_phys_addr & 0xFFF));
            status = ReadPhysicalSinglePage(TransferPageInfo, current_phys_addr, current_buffer, chunk_size);
            if (status != 0) return status;

            bytes_remaining -= chunk_size;
            current_phys_addr += chunk_size;
            current_buffer += chunk_size;
        }
        return 0; // 成功
    }
    else
    {
        // 4. 通用路径：操作跨越了物理页边界，使用保守的4KB分块循环
        PUCHAR currentBuffer = (PUCHAR)Buffer;
        ULONG64 currentVa = (ULONG64)Address;
        ULONG bytesRemaining = TotalSize;

        while (bytesRemaining > 0)
        {
            ULONG bytesToReadInThisPage = min(bytesRemaining, PAGE_SIZE - (ULONG)(currentVa & 0xFFF));

            // 我们需要重新调用GetPhysPageInfoStealth来翻译每个新的VA块
            // 但由于有缓存，后续调用会非常快
            status = GetPhysPageInfoStealth(TransferPageInfo, DirectoryTableBase, (PVOID)currentVa, &page_phys_base, &page_size, Context);
            if (status != 0) return status;

            const ULONG64 final_phys_addr = page_phys_base + (currentVa & (page_size - 1));

            status = ReadPhysicalSinglePage(TransferPageInfo, final_phys_addr, currentBuffer, bytesToReadInThisPage);
            if (status != 0) return status;

            bytesRemaining -= bytesToReadInThisPage;
            currentBuffer += bytesToReadInThisPage;
            currentVa += bytesToReadInThisPage;
        }
        return 0; // 成功
    }
}


__forceinline ULONG WriteVirtualMemory(
    const PHYSICAL_PAGE_INFO* TransferPageInfo,
    ULONG64 DirectoryTableBase,
    PVOID Address,
    PVOID Buffer,
    ULONG TotalSize,
    STEALTH_RW_CONTEXT* Context)
{
    if (!Address || !Buffer || !TotalSize || !TransferPageInfo || !Context || !DirectoryTableBase) return 22;
    if (KeGetCurrentIrql() > DISPATCH_LEVEL) return 261;

    // 1. 初始探测
    ULONG64 page_phys_base = 0;
    ULONG64 page_size = 0;
    ULONG status = GetPhysPageInfoStealth(TransferPageInfo, DirectoryTableBase, Address, &page_phys_base, &page_size, Context);
    if (status != 0) return status;

    // 2. 智能判断
    const ULONG64 offset_in_page = (ULONG64)Address & (page_size - 1);
    if ((offset_in_page + TotalSize) <= page_size)
    {
        // 3. 快速路径
        const ULONG64 start_phys_addr = page_phys_base + offset_in_page;
        ULONG bytes_remaining = TotalSize;
        ULONG64 current_phys_addr = start_phys_addr;
        PUCHAR current_buffer = (PUCHAR)Buffer;

        while (bytes_remaining > 0)
        {
            ULONG chunk_size = min(bytes_remaining, PAGE_SIZE - (ULONG)(current_phys_addr & 0xFFF));
            status = WritePhysicalSinglePage(TransferPageInfo, current_phys_addr, current_buffer, chunk_size);
            if (status != 0) return status;

            bytes_remaining -= chunk_size;
            current_phys_addr += chunk_size;
            current_buffer += chunk_size;
        }
        return 0;
    }
    else
    {
        // 4. 通用路径
        PUCHAR currentBuffer = (PUCHAR)Buffer;
        ULONG64 currentVa = (ULONG64)Address;
        ULONG bytesRemaining = TotalSize;

        while (bytesRemaining > 0)
        {
            ULONG bytesToWriteInThisPage = min(bytesRemaining, PAGE_SIZE - (ULONG)(currentVa & 0xFFF));

            status = GetPhysPageInfoStealth(TransferPageInfo, DirectoryTableBase, (PVOID)currentVa, &page_phys_base, &page_size, Context);
            if (status != 0) return status;

            const ULONG64 final_phys_addr = page_phys_base + (currentVa & (page_size - 1));

            status = WritePhysicalSinglePage(TransferPageInfo, final_phys_addr, currentBuffer, bytesToWriteInThisPage);
            if (status != 0) return status;

            bytesRemaining -= bytesToWriteInThisPage;
            currentBuffer += bytesToWriteInThisPage;
            currentVa += bytesToWriteInThisPage;
        }
        return 0;
    }
}