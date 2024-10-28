#pragma once

#include "types.h"

#define PML4_SELF_REF 255

typedef union _virt_addr_t
{
    u64 value;
    struct
    {
        u64 offset_4kb : 12;
        u64 pt_index : 9;
        u64 pd_index : 9;
        u64 pdpt_index : 9;
        u64 pml4_index : 9;
        u64 reserved : 16;
    };

    struct
    {
        u64 offset_2mb : 21;
        u64 pd_index : 9;
        u64 pdpt_index : 9;
        u64 pml4_index : 9;
        u64 reserved : 16;
    };

    struct
    {
        u64 offset_1gb : 30;
        u64 pdpt_index : 9;
        u64 pml4_index : 9;
        u64 reserved : 16;
    };

} virt_addr_t, * pvirt_addr_t;

using phys_addr_t = virt_addr_t;

enum class map_type { dest, src };

namespace hv 
{
    inline pml4e_64* vmxroot_pml4 = reinterpret_cast<pml4e_64*>(0x7fbfdfeff000);

	auto setup_page_tables() -> void;

    auto translate(virt_addr_t virt_addr)->u64;
    auto translate(virt_addr_t virt_addr, u64 pml4_phys, map_type type = map_type::src)->u64;

    auto map_page(u64 phys_addr, map_type type = map_type::src)->u64;
    auto map_virt(u64 dirbase, u64 virt_addr, map_type map_type = map_type::src)->u64;

    auto copy_virt(u64 dirbase_src, u64 virt_src, u64 dirbase_dest, u64 virt_dest, u64 size) -> bool;
}

