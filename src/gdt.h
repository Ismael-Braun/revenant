#pragma once

#include <ia32.hpp>

namespace hv
{
	inline constexpr segment_selector host_cs_selector = { 0, 0, 1 };
	inline constexpr segment_selector host_tr_selector = { 0, 0, 2 };
	inline constexpr size_t host_gdt_descriptor_count = 4;

	void prepare_host_gdt(segment_descriptor_32* gdt, task_state_segment_64 const* tss);
}

