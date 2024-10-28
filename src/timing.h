#pragma once

#include "vcpu.h"
#include "types.h"

namespace hv
{
	auto hide_vm_exit_overhead(vcpu_t* cpu) -> void;
	auto measure_vm_exit_tsc_overhead() -> u64;
	auto measure_vm_exit_ref_tsc_overhead() -> u64;
	auto measure_vm_exit_mperf_overhead() -> u64;
}

