#include "mm.h"
#include "vcpu.h"
#include "hv.h"

using namespace vmx;

namespace hv
{
	auto setup_page_tables() -> void
	{
		cr3 cr3_value;
		cr3_value.flags = 0;
		cr3_value.address_of_page_directory = (MmGetPhysicalAddress(&ghv.page_table_pml4).QuadPart >> 12);

		memset(ghv.page_table_pml4, NULL, sizeof ghv.page_table_pml4);
		ghv.page_table_pml4[PML4_SELF_REF].page_frame_number = cr3_value.address_of_page_directory;
		ghv.page_table_pml4[PML4_SELF_REF].present = true;
		ghv.page_table_pml4[PML4_SELF_REF].write = true;
		ghv.page_table_pml4[PML4_SELF_REF].supervisor = false;

		PHYSICAL_ADDRESS pml4_address;
		pml4_address.QuadPart = ghv.system_cr3.address_of_page_directory << 12;

		auto const guest_pml4 = static_cast<pml4e_64*>(MmGetVirtualForPhysical(pml4_address));

		memcpy(&ghv.page_table_pml4[256], &guest_pml4[256], sizeof(pml4e_64) * 256);

		for (auto idx = 0u; idx < 255; ++idx)
		{
			reinterpret_cast<pte_64*>(ghv.page_table_pml4)[idx].present = true;
			reinterpret_cast<pte_64*>(ghv.page_table_pml4)[idx].write = true;
		}

		ghv.page_table_cr3 = cr3_value;

		log_info("page table cr3 -> %p", ghv.page_table_cr3.flags);
	}

    auto translate(virt_addr_t virt_addr) -> u64
    {
        virt_addr_t cursor{ (u64)vmxroot_pml4 };

        if (!reinterpret_cast<pml4e_64*>(cursor.value)[virt_addr.pml4_index].present)
            return {};

        cursor.pt_index = virt_addr.pml4_index;
        if (!reinterpret_cast<pdpte_64*>(cursor.value)[virt_addr.pdpt_index].present)
            return {};

        // handle 1gb large page...
        if (reinterpret_cast<pdpte_64*>(cursor.value)[virt_addr.pdpt_index].large_page)
            return (reinterpret_cast<pdpte_64*>(cursor.value)
                [virt_addr.pdpt_index].page_frame_number << 12) + virt_addr.offset_1gb;


        cursor.pd_index = virt_addr.pml4_index;
        cursor.pt_index = virt_addr.pdpt_index;
        if (!reinterpret_cast<pde_64*>(cursor.value)[virt_addr.pd_index].present)
            return {};

        // handle 2mb large page...
        if (reinterpret_cast<pde_64*>(cursor.value)[virt_addr.pd_index].large_page)
            return (reinterpret_cast<pde_64*>(cursor.value)
                [virt_addr.pd_index].page_frame_number << 12) + virt_addr.offset_2mb;


        cursor.pdpt_index = virt_addr.pml4_index;
        cursor.pd_index = virt_addr.pdpt_index;
        cursor.pt_index = virt_addr.pd_index;
        if (!reinterpret_cast<pte_64*>(cursor.value)[virt_addr.pt_index].present)
            return {};

        return (reinterpret_cast<pte_64*>(cursor.value)
            [virt_addr.pt_index].page_frame_number << 12) + virt_addr.offset_4kb;
    }

    auto translate(virt_addr_t virt_addr, u64 pml4_phys, map_type type) -> u64
    {
        const auto pml4 =
            reinterpret_cast<pml4e_64*>(
                map_page(pml4_phys, type));

        if (!pml4[virt_addr.pml4_index].present)
            return {};

        const auto pdpt =
            reinterpret_cast<pdpte_64*>(
                map_page(pml4[virt_addr
                    .pml4_index].page_frame_number << 12, type));

        if (!pdpt[virt_addr.pdpt_index].present)
            return {};

        if (pdpt[virt_addr.pdpt_index].large_page)
            return (pdpt[virt_addr.pdpt_index].page_frame_number << 12) + virt_addr.offset_1gb;

        const auto pd =
            reinterpret_cast<pde_64*>(
                map_page(pdpt[virt_addr
                    .pdpt_index].page_frame_number << 12, type));

        if (!pd[virt_addr.pd_index].present)
            return {};

        if (pd[virt_addr.pd_index].large_page)
            return (pd[virt_addr.pd_index].page_frame_number << 12) + virt_addr.offset_2mb;

        const auto pt =
            reinterpret_cast<pte_64*>(
                map_page(pd[virt_addr
                    .pd_index].page_frame_number << 12, type));

        if (!pt[virt_addr.pt_index].present)
            return {};

        return (pt[virt_addr.pt_index].page_frame_number << 12) + virt_addr.offset_4kb;
    }

    auto map_page(u64 phys_addr, map_type type) -> u64
    {
        cpuid_eax_01 cpuid_value;
        virt_addr_t result{ (u64)vmxroot_pml4 };
        __cpuid((int*)&cpuid_value, 1);

        result.pt_index = (cpuid_value
            .cpuid_additional_information
            .initial_apic_id * 2)
            + (unsigned)type;

        reinterpret_cast<pte_64*>(vmxroot_pml4)
            [result.pt_index].page_frame_number = phys_addr >> 12;

        __invlpg((void*)result.value);
        result.offset_4kb = phys_addr_t{ phys_addr }.offset_4kb;
        return result.value;
    }

    auto map_virt(u64 dirbase, u64 virt_addr, map_type map_type) -> u64
    {
        const auto phys_addr =
            translate(virt_addr_t{ virt_addr },
                dirbase, map_type);

        if (!phys_addr)
            return {};

        return map_page(phys_addr, map_type);
    }

    auto copy_virt(u64 dirbase_src, u64 virt_src, u64 dirbase_dest, u64 virt_dest, u64 size) -> bool
    {
        while (size)
        {
            auto dest_size = PAGE_SIZE - virt_addr_t{ virt_dest }.offset_4kb;
            if (size < dest_size)
                dest_size = size;

            auto src_size = PAGE_SIZE - virt_addr_t{ virt_src }.offset_4kb;
            if (size < src_size)
                src_size = size;

            const auto mapped_src =
                reinterpret_cast<void*>(
                    map_virt(dirbase_src, virt_src, map_type::src));

            if (!mapped_src)
                return false;

            const auto mapped_dest =
                reinterpret_cast<void*>(
                    map_virt(dirbase_dest, virt_dest, map_type::dest));

            if (!mapped_dest)
                return false;

            // copy directly between the two pages...
            auto current_size = min(dest_size, src_size);

            __try { memcpy(mapped_dest, mapped_src, current_size); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }

            virt_src += current_size;
            virt_dest += current_size;
            size -= current_size;
        }

        return true;
    }

}
