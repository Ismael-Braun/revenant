#include "mtrr.h"

#define MTRR_PAGE_SIZE 4096

namespace mtrr
{
	auto initialize_mtrr(mtrr_list& mtrr_data) -> void
	{
		ia32_mtrr_capabilities_register mtrr_capabilities{};
		mtrr_capabilities.flags = __readmsr(IA32_MTRR_CAPABILITIES);

		for (auto i = 0u; i < mtrr_capabilities.variable_range_count; i++)
		{
			ia32_mtrr_physbase_register mtrr_base{};
			ia32_mtrr_physmask_register mtrr_mask{};

			mtrr_base.flags = __readmsr(IA32_MTRR_PHYSBASE0 + i * 2);
			mtrr_mask.flags = __readmsr(IA32_MTRR_PHYSMASK0 + i * 2);

			mtrr_data[i].type = static_cast<uint32_t>(mtrr_base.type);
			mtrr_data[i].enabled = static_cast<uint32_t>(mtrr_mask.valid);
			if (mtrr_data[i].enabled != FALSE)
			{
				mtrr_data[i].physical_address_min = mtrr_base.page_frame_number *
					MTRR_PAGE_SIZE;

				unsigned long bit{};
				_BitScanForward64(&bit, mtrr_mask.page_frame_number * MTRR_PAGE_SIZE);
				mtrr_data[i].physical_address_max = mtrr_data[i].
					physical_address_min +
					(1ULL << bit) - 1;
			}
		}
	}

	auto mtrr_adjust_effective_memory_type(const mtrr_list& mtrr_data, const u64 large_page_address,
		u32 candidate_memory_type)->u32
	{
		for (const auto& mtrr_entry : mtrr_data)
		{
			if (mtrr_entry.enabled && large_page_address + (2_mb - 1) >= mtrr_entry.physical_address_min &&
				large_page_address <= mtrr_entry.physical_address_max)
			{
				candidate_memory_type = mtrr_entry.type;
			}
		}

		return candidate_memory_type;
	}
}