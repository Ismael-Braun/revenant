#pragma once

#include <ia32.hpp>

namespace hv
{
	inline constexpr size_t host_idt_descriptor_count = 256;

	void prepare_host_idt(segment_descriptor_interrupt_gate_64* idt);
}

