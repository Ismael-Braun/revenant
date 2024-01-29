#include "ept.h"
#include "mtrr.h"
#include "hv.h"
#include "vmx.h"

#define ADDRMASK_EPT_PML1_OFFSET(_VAR_) ((_VAR_) & 0xFFFULL)
#define ADDRMASK_EPT_PML1_INDEX(_VAR_) (((_VAR_) & 0x1FF000ULL) >> 12)
#define ADDRMASK_EPT_PML2_INDEX(_VAR_) (((_VAR_) & 0x3FE00000ULL) >> 21)
#define ADDRMASK_EPT_PML3_INDEX(_VAR_) (((_VAR_) & 0x7FC0000000ULL) >> 30)
#define ADDRMASK_EPT_PML4_INDEX(_VAR_) (((_VAR_) & 0xFF8000000000ULL) >> 39)

using namespace mtrr;

auto ept_t::get_ept_pointer() -> ept_pointer
{
	ept_pointer vmx_eptp{};
	vmx_eptp.flags = 0;
	vmx_eptp.page_walk_length = 3;
	vmx_eptp.memory_type = MEMORY_TYPE_WRITE_BACK;
	vmx_eptp.page_frame_number = this->plm4_phys / PAGE_SIZE;

	return vmx_eptp;
}

auto ept_t::start() -> void
{
	this->used_free_pages = 0;
	this->dummy_page_pfn = MmGetPhysicalAddress(this->dummy_page).QuadPart >> 12;
	this->plm4_phys = MmGetPhysicalAddress(const_cast<ept_pml4e*>(&this->pml4[0])).QuadPart;

	this->hook_count = 0;
	this->hook_list = reinterpret_cast<ept_hook*>(ExAllocatePoolZero(NonPagedPool, sizeof(ept_hook) * MAX_EPT_HOOKS, HV_POOL_TAG));

	mtrr_list mtrr_data{};
	initialize_mtrr(mtrr_data);

	this->pml4[0].read_access = 1;
	this->pml4[0].write_access = 1;
	this->pml4[0].execute_access = 1;
	this->pml4[0].page_frame_number = MmGetPhysicalAddress(&this->pdpt).QuadPart / PAGE_SIZE;

	ept_pdpte temp_epdpte;
	temp_epdpte.flags = 0;
	temp_epdpte.read_access = 1;
	temp_epdpte.write_access = 1;
	temp_epdpte.execute_access = 1;

	__stosq(reinterpret_cast<uint64_t*>(&this->pdpt[0]), temp_epdpte.flags, EPT_PDPTE_ENTRY_COUNT);

	for (auto i = 0; i < EPT_PDPTE_ENTRY_COUNT; i++)
	{
		this->pdpt[i].page_frame_number = MmGetPhysicalAddress(&this->pde[i][0]).QuadPart / PAGE_SIZE;
	}

	ept_pde_2mb temp_epde{};
	temp_epde.flags = 0;
	temp_epde.read_access = 1;
	temp_epde.write_access = 1;
	temp_epde.execute_access = 1;
	temp_epde.large_page = 1;

	__stosq(reinterpret_cast<uint64_t*>(this->pde), temp_epde.flags, EPT_PDPTE_ENTRY_COUNT * EPT_PDE_ENTRY_COUNT);

	for (auto i = 0; i < EPT_PDPTE_ENTRY_COUNT; i++)
	{
		for (auto j = 0; j < EPT_PDE_ENTRY_COUNT; j++)
		{
			this->pde[i][j].page_frame_number = (i * 512) + j;
			this->pde[i][j].memory_type = mtrr_adjust_effective_memory_type(
				mtrr_data, this->pde[i][j].page_frame_number * 2_mb, MEMORY_TYPE_WRITE_BACK);
		}
	}


	log_info("ept started!");
}

auto ept_t::invalidate() -> void
{
	auto ept_pointer = this->get_ept_pointer();

	invept_descriptor descriptor{};
	descriptor.ept_pointer = ept_pointer.flags;
	descriptor.reserved = 0;

	vmx::invept(invept_single_context, descriptor);
}

auto ept_t::get_pde_2mb(u64 phys) -> ept_pde_2mb*
{
	const auto directory = ADDRMASK_EPT_PML2_INDEX(phys);
	const auto directory_pointer = ADDRMASK_EPT_PML3_INDEX(phys);
	const auto pml4_entry = ADDRMASK_EPT_PML4_INDEX(phys);

	if (pml4_entry > 0)
	{
		return nullptr;
	}

	return &this->pde[directory_pointer][directory];
}

auto ept_t::get_pte(u64 phys) -> ept_pte*
{
	auto* pde_2mb = this->get_pde_2mb(phys);

	if (!pde_2mb || pde_2mb->large_page)
		return nullptr;

	const auto* pde = reinterpret_cast<ept_pde*>(pde_2mb);

	PHYSICAL_ADDRESS physical_address{};
	physical_address.QuadPart = static_cast<LONGLONG>(pde->page_frame_number * PAGE_SIZE);

	auto* pte = static_cast<ept_pte*>(MmGetVirtualForPhysical(physical_address));

	if (!pte)
		return nullptr;

	return &pte[ADDRMASK_EPT_PML1_INDEX(phys)];
}

auto ept_t::split_large_page(u64 phys) -> bool
{
	auto* pde_2mb = this->get_pde_2mb(phys);

	if (!pde_2mb)
		return false;

	if (!pde_2mb->large_page)
		return true;

	if (this->used_free_pages >= FREE_PAGES_SIZE)
		return false;

	auto const split = reinterpret_cast<ept_split*>(
		&this->free_pages[this->used_free_pages]);

	if (!split)
		return false;

	++this->used_free_pages;

	ept_pte pml1_template{};
	pml1_template.flags = 0;
	pml1_template.read_access = 1;
	pml1_template.write_access = 1;
	pml1_template.execute_access = 1;
	pml1_template.memory_type = pde_2mb->memory_type;
	pml1_template.ignore_pat = pde_2mb->ignore_pat;
	pml1_template.suppress_ve = pde_2mb->suppress_ve;

	__stosq(reinterpret_cast<uint64_t*>(&split->pte[0]), pml1_template.flags, EPT_PTE_ENTRY_COUNT);

	for (auto i = 0; i < EPT_PTE_ENTRY_COUNT; ++i)
	{
		split->pte[i].page_frame_number = ((pde_2mb->page_frame_number * 2_mb) / PAGE_SIZE) + i;
	}

	ept_pde new_pointer{};
	new_pointer.flags = 0;
	new_pointer.read_access = 1;
	new_pointer.write_access = 1;
	new_pointer.execute_access = 1;

	new_pointer.page_frame_number = MmGetPhysicalAddress(&split->pte[0]).QuadPart / PAGE_SIZE;

	pde_2mb->flags = new_pointer.flags;

	return true;
}

auto ept_t::find_ept_hook(void* phys_addr)->ept_hook*
{
	for (int i = 0; i < this->hook_count; ++i)
	{
		if (this->hook_list[i].physical_address == phys_addr)
		{
			return &this->hook_list[i];
		}
	}

	return nullptr;
}

auto ept_t::remove_ept_hook(void* virt_addr) -> PMDL
{
	for (int i = 0; i < this->hook_count; ++i)
	{
		if (this->hook_list[i].virtual_address == virt_addr)
		{
			auto hook = &this->hook_list[i];

			hook->target_page->flags = hook->original_page.flags;

			auto ret = hook->mdl;

			memmove(this->hook_list + this->hook_count - 1, this->hook_list + this->hook_count,
				MAX_EPT_HOOKS - this->hook_count);

			this->hook_count -= 1;

			invalidate();

			return ret;
		}
	}

	return nullptr;
}

auto ept_t::install_page_hook(void* addr, u8* patch, size_t patch_size, ept_hint* hint, PMDL mdl) -> bool
{
	auto vmroot_cr3 = __readcr3();
	cr3 guest_cr3;

	guest_cr3.flags = vmx::vm_read(VMCS_GUEST_CR3);

	auto physical_page = PAGE_ALIGN(translate(virt_addr_t{ (u64)addr }, guest_cr3.address_of_page_directory << 12));

	if (!physical_page)
		return false;

	__writecr3(ghv.system_cr3.flags);

	ept_hook* hook_entry = find_ept_hook(physical_page);

	if (hook_entry)
	{
		if (hook_entry->target_page->flags == hook_entry->original_page.flags)
		{
			memcpy(&hook_entry->fake_page[0], &hint->page[0], PAGE_SIZE);

			hook_entry->target_page->flags = hook_entry->rw_page.flags;
		}

		auto page_offset = (uintptr_t)addr & (PAGE_SIZE - 1);
		memcpy(hook_entry->fake_page + page_offset, patch, patch_size);

		return true;
	}

	if (this->hook_count >= MAX_EPT_HOOKS)
		return false;

	if (!this->split_large_page((u64)physical_page))
		return false;

	hook_entry = &this->hook_list[this->hook_count];

	this->hook_count += 1;

	hook_entry->mdl = mdl;
	hook_entry->physical_address = physical_page;
	hook_entry->virtual_address = addr;

	hook_entry->target_page = this->get_pte((u64)physical_page);

	if (!hook_entry->target_page)
		return false;

	memcpy(&hook_entry->fake_page[0], &hint->page[0], PAGE_SIZE);

	hook_entry->original_page = *hook_entry->target_page;
	hook_entry->rw_page = hook_entry->original_page;

	hook_entry->rw_page.read_access = 1;
	hook_entry->rw_page.write_access = 1;
	hook_entry->rw_page.execute_access = 0;

	hook_entry->exec_page.flags = 0;
	hook_entry->exec_page.read_access = 0;
	hook_entry->exec_page.write_access = 0;
	hook_entry->exec_page.execute_access = 1;
	hook_entry->exec_page.page_frame_number = MmGetPhysicalAddress(&hook_entry->fake_page).QuadPart / PAGE_SIZE;

	hook_entry->target_page->flags = hook_entry->rw_page.flags;

	auto page_offset = (uintptr_t)addr & (PAGE_SIZE - 1);
	memcpy(hook_entry->fake_page + page_offset, patch, patch_size);

	__writecr3(vmroot_cr3);

	return true;
}