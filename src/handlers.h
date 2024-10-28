#pragma once

#include <ia32.hpp>
#include "vcpu.h"

namespace handlers
{
	auto cpuid(vcpu_t* cpu) -> void;
	auto rdmsr(vcpu_t* cpu) -> void;
	auto wrmsr(vcpu_t* cpu) -> void;
	auto getsec(vcpu_t* cpu) -> void;
	auto invd(vcpu_t* cpu) -> void;
	auto xsetbv(vcpu_t* cpu) -> void;
	auto vmxon(vcpu_t* cpu) -> void;
	auto vmcall(vcpu_t* cpu) -> void;
	auto vmx_preemption(vcpu_t* cpu) -> void;
	auto mov_to_cr0(vcpu_t* cpu, u64 gpr) -> void;
	auto mov_to_cr3(vcpu_t* cpu, u64 gpr) -> void;
	auto mov_to_cr4(vcpu_t* cpu, u64 gpr) -> void;
	auto mov_from_cr3(vcpu_t* cpu, u64 gpr) -> void;
	auto clts(vcpu_t* cpu) -> void;
	auto lmsw(vcpu_t* cpu, u16 value) -> void;
	auto mov_cr(vcpu_t* cpu) -> void;
	auto nmi_window(vcpu_t* cpu) -> void;
	auto exception_or_nmi(vcpu_t* cpu) -> void;
	auto vmx_instruction(vcpu_t* cpu) -> void;
	auto rdtsc(vcpu_t* cpu) -> void;
	auto rdtscp(vcpu_t* cpu) -> void;
	auto ept_violation(vcpu_t* cpu) -> void;
	auto ept_misconfiguration(vcpu_t* cpu) -> void;
}

