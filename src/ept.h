#pragma once

#include "types.h"

#define FREE_PAGES_SIZE 6000
#define MAX_EPT_HOOKS   6000

typedef struct ept_split
{
	alignas(PAGE_SIZE) ept_pte pte[EPT_PTE_ENTRY_COUNT]{};
};

typedef struct ept_hint
{
	alignas(PAGE_SIZE) u8 page_copy[PAGE_SIZE];
	u64 physical_addr;
	u64 virtual_addr;
	u8* patch;
	size_t patch_size;
	MDL* mdl;
};

typedef struct ept_hook
{
	alignas(PAGE_SIZE) u8 fake_page[PAGE_SIZE];

	void* physical_address;
	void* virtual_address;
	MDL* mdl;

	ept_pte* target_page;
	ept_pte original_page;
	ept_pte rw_page;
	ept_pte exec_page;
};

class ept_t
{
public:
	auto start() -> void;
	auto get_ept_pointer()->ept_pointer;

	auto invalidate() -> void;

	auto get_pde_2mb(u64 phys)->ept_pde_2mb*;
	auto get_pte(u64 phys)->ept_pte*;

	auto split_large_page(u64 phys) -> bool;

	auto find_ept_hook(void* phys_addr)->ept_hook*;
	auto remove_ept_hook(void* virt_addr) -> bool;

	auto install_page_hook(ept_hint* hint) -> bool;

	ept_hook* hook_list;
	u64 hook_count;

	u64 plm4_phys;

	u64 dummy_page_pfn;
	size_t used_free_pages;

	alignas(PAGE_SIZE) ept_pml4e pml4[EPT_PML4E_ENTRY_COUNT];
	alignas(PAGE_SIZE) ept_pdpte pdpt[EPT_PDPTE_ENTRY_COUNT];
	alignas(PAGE_SIZE) ept_pde_2mb pde[EPT_PDPTE_ENTRY_COUNT][EPT_PDE_ENTRY_COUNT];

	alignas(PAGE_SIZE) uint8_t free_pages[FREE_PAGES_SIZE][PAGE_SIZE];
	alignas(PAGE_SIZE) uint8_t dummy_page[PAGE_SIZE];
};