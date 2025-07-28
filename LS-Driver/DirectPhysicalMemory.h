#pragma once
#include <ntifs.h>
#include <intrin.h>

// --- �ṹ�嶨�� ---

// �������������ڴ��������תҳ��Ϣ
struct PHYSICAL_PAGE_INFO
{
    PVOID BaseAddress;
    SIZE_T Size;
    PVOID PteAddress;
};

// �ռ����η����������Ľṹ�壺��������ҳ�����ֵ
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


// --- ȫ�ֱ��� ---
static ULONG64 g_PteBaseForAlloc = 0;
static BOOLEAN g_IsInitPteBaseForAlloc = false;
static PPHYSICAL_MEMORY_RANGE g_PhysicalMemoryRanges = 0;


// --- ���ĸ�����������Դ���� ---

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


//��תҳ�����ⲿ����
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


//��Ŀ�������ַӳ�䵽��תҳ�������ж�д������ת
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


//����Ҫ�Ļ������
__forceinline ULONG GetPhysPageInfoStealth(const PHYSICAL_PAGE_INFO* TransferPageInfo,ULONG64 Cr3,PVOID Va,PULONG64 pPhysicalPageBase,PULONG64 pPageSize,STEALTH_RW_CONTEXT* Context)
{
    if (!pPhysicalPageBase || !pPageSize || !Context) return 22;

    const ULONG64 va_val = (ULONG64)Va;
    const ULONG64 pxe_va_range = va_val >> 39;
    const ULONG64 ppe_va_range = va_val >> 30;
    const ULONG64 pde_va_range = va_val >> 21;

    ULONG64 pxe_val, ppe_val, pde_val, pte_val;

    // 1. ���CR3������仯��������л���
    if (Context->TargetCr3 != Cr3)
    {
        memset(Context, 0, sizeof(STEALTH_RW_CONTEXT));
        Context->TargetCr3 = Cr3;
    }

    // 2. PXE (PML4E) ������
    if (Context->CachedPxeVaRange != pxe_va_range)
    {
        // ֻҪPXE���ˣ������һ�ж��������¼���
        if (ReadPhysicalSinglePage(TransferPageInfo, (Cr3 & ~0xFFF) + 8 * (pxe_va_range & 0x1FF), &pxe_val, 8) || (pxe_val & 1) == 0) return 262;
        Context->CachedPxeVal = pxe_val;
        Context->CachedPxeVaRange = pxe_va_range;
        Context->CachedPpeVaRange = (ULONG64)-1; // ǿ���¼�ʧЧ
        Context->CachedPdeVaRange = (ULONG64)-1;
    }
    pxe_val = Context->CachedPxeVal;

    // 3. PPE (PDPTE) ������
    if (Context->CachedPpeVaRange != ppe_va_range)
    {
        const ULONG64 ppe_phys_addr = (pxe_val & 0x000FFFFFFFFF000) + 8 * (ppe_va_range & 0x1FF);
        if (ReadPhysicalSinglePage(TransferPageInfo, ppe_phys_addr, &ppe_val, 8) || (ppe_val & 1) == 0) return 263;
        Context->CachedPpeVal = ppe_val;
        Context->CachedPpeVaRange = ppe_va_range;
        Context->CachedPdeVaRange = (ULONG64)-1; // ǿ���¼�ʧЧ
    }
    ppe_val = Context->CachedPpeVal;
    if (ppe_val & 0x80) { *pPageSize = 0x40000000; *pPhysicalPageBase = (ppe_val & 0x000FFFFFC0000000); return 0; }

    // 4. PDE ������
    if (Context->CachedPdeVaRange != pde_va_range)
    {
        const ULONG64 pde_phys_addr = (ppe_val & 0x000FFFFFFFFF000) + 8 * (pde_va_range & 0x1FF);
        if (ReadPhysicalSinglePage(TransferPageInfo, pde_phys_addr, &pde_val, 8) || (pde_val & 1) == 0) return 264;
        Context->CachedPdeVal = pde_val;
        Context->CachedPdeVaRange = pde_va_range;
    }
    pde_val = Context->CachedPdeVal;
    if (pde_val & 0x80) { *pPageSize = 0x200000; *pPhysicalPageBase = (pde_val & 0x000FFFFFFE00000); return 0; }

    // 5. PTE �����޻��棬����ֱ�Ӷ�ȡ��
    const ULONG64 pte_phys_addr = (pde_val & 0x000FFFFFFFFF000) + 8 * ((va_val >> 12) & 0x1FF);
    if (ReadPhysicalSinglePage(TransferPageInfo, pte_phys_addr, &pte_val, 8) || (pte_val & 1) == 0) return 265;

    *pPageSize = 0x1000;
    *pPhysicalPageBase = (pte_val & 0x000FFFFFFFFF000);
    return 0;
}

//����Ҫ�Ļ�����ƣ����Ժ���
__forceinline ULONG GetPhysPageInfoStealth�������(const PHYSICAL_PAGE_INFO* TransferPageInfo,ULONG64 Cr3,PVOID Va,PULONG64 pPhysicalPageBase,PULONG64 pPageSize,STEALTH_RW_CONTEXT* Context)
{
    if (!pPhysicalPageBase || !pPageSize || !Context) return 22;

    const ULONG64 va_val = (ULONG64)Va;
    const ULONG64 pxe_va_range = va_val >> 39;
    const ULONG64 ppe_va_range = va_val >> 30;
    const ULONG64 pde_va_range = va_val >> 21;

    ULONG64 pxe_val, ppe_val, pde_val, pte_val;

    // --- ���Դ�ӡ: ������� ---
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
        "[MyDriver] GetPhys: VA=%p, CR3=%llX. Cache Check Start...\n", Va, Cr3);

    // 1. ���CR3������仯��������л���
    if (Context->TargetCr3 != Cr3)
    {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: CR3 changed! Invalidating all cache.\n");
        memset(Context, 0, sizeof(STEALTH_RW_CONTEXT));
        Context->TargetCr3 = Cr3;
    }

    // 2. PXE (PML4E) ��������
    if (Context->CachedPxeVaRange == pxe_va_range)
    {
        pxe_val = Context->CachedPxeVal; // ���л���
        // --- ���Դ�ӡ: PXE ���� ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PXE Cache HIT for range %llX.\n", pxe_va_range);
    }
    else
    {
        // --- ���Դ�ӡ: PXE δ���� ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PXE Cache MISS for range %llX. Reading physical...\n", pxe_va_range);

        // δ���У���CR3�����ȡ
        if (ReadPhysicalSinglePage(TransferPageInfo, (Cr3 & ~0xFFF) + 8 * (pxe_va_range & 0x1FF), &pxe_val, 8) || (pxe_val & 1) == 0) return 262;

        // ���»���
        Context->CachedPxeVal = pxe_val;
        Context->CachedPxeVaRange = pxe_va_range;
        Context->CachedPpeVaRange = (ULONG64)-1; // �¼�����ʧЧ
        Context->CachedPdeVaRange = (ULONG64)-1;
    }

    // 3. PPE (PDPTE) ��������
    if (Context->CachedPpeVaRange == ppe_va_range)
    {
        ppe_val = Context->CachedPpeVal; // ���л���
        // --- ���Դ�ӡ: PPE ���� ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PPE Cache HIT for range %llX.\n", ppe_va_range);
    }
    else
    {
        // --- ���Դ�ӡ: PPE δ���� ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PPE Cache MISS for range %llX. Reading physical...\n", ppe_va_range);

        // δ���У���PXEָ�������ҳ��ȡ
        const ULONG64 ppe_phys_addr = (pxe_val & 0x000FFFFFFFFF000) + 8 * (ppe_va_range & 0x1FF);
        if (ReadPhysicalSinglePage(TransferPageInfo, ppe_phys_addr, &ppe_val, 8) || (ppe_val & 1) == 0) return 263;

        // ���»���
        Context->CachedPpeVal = ppe_val;
        Context->CachedPpeVaRange = ppe_va_range;
        Context->CachedPdeVaRange = (ULONG64)-1; // �¼�����ʧЧ
    }
    if (ppe_val & 0x80) { *pPageSize = 0x40000000; *pPhysicalPageBase = (ppe_val & 0x000FFFFFC0000000); return 0; }

    // 4. PDE ��������
    if (Context->CachedPdeVaRange == pde_va_range)
    {
        pde_val = Context->CachedPdeVal; // ���л���
        // --- ���Դ�ӡ: PDE ���� ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PDE Cache HIT for range %llX.\n", pde_va_range);
    }
    else
    {
        // --- ���Դ�ӡ: PDE δ���� ---
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL,
            "[MyDriver] GetPhys: PDE Cache MISS for range %llX. Reading physical...\n", pde_va_range);

        // δ���У���PPEָ�������ҳ��ȡ
        const ULONG64 pde_phys_addr = (ppe_val & 0x000FFFFFFFFF000) + 8 * (pde_va_range & 0x1FF);
        if (ReadPhysicalSinglePage(TransferPageInfo, pde_phys_addr, &pde_val, 8) || (pde_val & 1) == 0) return 264;

        // ���»���
        Context->CachedPdeVal = pde_val;
        Context->CachedPdeVaRange = pde_va_range;
    }
    if (pde_val & 0x80) { *pPageSize = 0x200000; *pPhysicalPageBase = (pde_val & 0x000FFFFFFE00000); return 0; }

    // 5. PTE �����޻��棬����ֱ�Ӷ�ȡ��
    const ULONG64 pte_phys_addr = (pde_val & 0x000FFFFFFFFF000) + 8 * ((va_val >> 12) & 0x1FF);
    if (ReadPhysicalSinglePage(TransferPageInfo, pte_phys_addr, &pte_val, 8) || (pte_val & 1) == 0) return 265;

    *pPageSize = 0x1000;
    *pPhysicalPageBase = (pte_val & 0x000FFFFFFFFF000);
    return 0;
}

// --- ���յġ�ͳһ�ġ����ܵĶ�д�ӿ� ---

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

    // 1. ��ʼ̽��
    ULONG64 page_phys_base = 0;
    ULONG64 page_size = 0;
    ULONG status = GetPhysPageInfoStealth(TransferPageInfo, DirectoryTableBase, Address, &page_phys_base, &page_size, Context);
    if (status != 0) return status;

    // 2. �����ж�
    const ULONG64 offset_in_page = (ULONG64)Address & (page_size - 1);
    if ((offset_in_page + TotalSize) <= page_size)
    {
        // 3. ����·����������ȡ��������һ������ҳ��
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
        return 0; // �ɹ�
    }
    else
    {
        // 4. ͨ��·����������Խ������ҳ�߽磬ʹ�ñ��ص�4KB�ֿ�ѭ��
        PUCHAR currentBuffer = (PUCHAR)Buffer;
        ULONG64 currentVa = (ULONG64)Address;
        ULONG bytesRemaining = TotalSize;

        while (bytesRemaining > 0)
        {
            ULONG bytesToReadInThisPage = min(bytesRemaining, PAGE_SIZE - (ULONG)(currentVa & 0xFFF));

            // ������Ҫ���µ���GetPhysPageInfoStealth������ÿ���µ�VA��
            // �������л��棬�������û�ǳ���
            status = GetPhysPageInfoStealth(TransferPageInfo, DirectoryTableBase, (PVOID)currentVa, &page_phys_base, &page_size, Context);
            if (status != 0) return status;

            const ULONG64 final_phys_addr = page_phys_base + (currentVa & (page_size - 1));

            status = ReadPhysicalSinglePage(TransferPageInfo, final_phys_addr, currentBuffer, bytesToReadInThisPage);
            if (status != 0) return status;

            bytesRemaining -= bytesToReadInThisPage;
            currentBuffer += bytesToReadInThisPage;
            currentVa += bytesToReadInThisPage;
        }
        return 0; // �ɹ�
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

    // 1. ��ʼ̽��
    ULONG64 page_phys_base = 0;
    ULONG64 page_size = 0;
    ULONG status = GetPhysPageInfoStealth(TransferPageInfo, DirectoryTableBase, Address, &page_phys_base, &page_size, Context);
    if (status != 0) return status;

    // 2. �����ж�
    const ULONG64 offset_in_page = (ULONG64)Address & (page_size - 1);
    if ((offset_in_page + TotalSize) <= page_size)
    {
        // 3. ����·��
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
        // 4. ͨ��·��
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