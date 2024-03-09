#pragma once

#include "types.h"
#include "guest_registers.h"
#include "vcpu.h"

namespace hypercalls
{
	inline constexpr u64 hv_signature = 'rvnt';
	inline constexpr u64 hv_key = 0x21167;

	enum command : u64
	{
		hypercall_ping = 0x55,
		hypercall_hv_base,
		hypercall_hide_physical_page,
		hypercall_instal_ept_hook,
		hypercall_remove_ept_hook,
		hypercall_current_dirbase,
		hypercall_copy_virtual_memory
	};

	typedef struct input
	{
		// rax
		struct
		{
			command        code : 8;
			u64            key : 56;
		};

		// rcx, rdx, r8, r9, r10, r11
		u64 args[6];
	};

	u64 vmx_vmcall(input& hv_input);

	auto ping(vcpu_t* vcpu) -> void;
	auto hv_base(vcpu_t* vcpu) -> void;
	auto hide_physical_page(vcpu_t* vcpu) -> void;
	auto install_ept_hook(vcpu_t* vcpu) -> void;
	auto remove_ept_hook(vcpu_t* vcpu) -> void;
	auto current_dirbase(vcpu_t* vcpu) -> void;
	auto copy_memory(vcpu_t* vcpu) -> void;
}

