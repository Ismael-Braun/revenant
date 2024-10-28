#pragma once

#include "types.h"

namespace mtrr
{
	inline u64 operator"" _kb(const uint64_t size)
	{
		return size * 1024;
	}

	inline u64 operator"" _mb(const uint64_t size)
	{
		return size * 1024_kb;
	}

	inline u64 operator"" _gb(const uint64_t size)
	{
		return size * 1024_mb;
	}

	struct mtrr_range
	{
		u32 enabled;
		u32 type;
		u64 physical_address_min;
		u64 physical_address_max;
	};

	using mtrr_list = mtrr_range[16]; //update this to 64?

	auto initialize_mtrr(mtrr_list& mtrr_data) -> void;
	auto mtrr_adjust_effective_memory_type(const mtrr_list& mtrr_data, const u64 large_page_address,
		u32 candidate_memory_type)->u32;
}