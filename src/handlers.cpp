#include "handlers.h"
#include "guest_registers.h"
#include "vcpu.h"
#include "vmx.h"
#include "hv.h"
#include "hypercalls.h"

using namespace vmx;

namespace handlers
{
	auto cpuid(vcpu_t* cpu) -> void
	{
		auto const ctx = cpu->ctx;

		int regs[4];
		__cpuidex(regs, ctx->eax, ctx->ecx);

		ctx->rax = regs[0];
		ctx->rbx = regs[1];
		ctx->rcx = regs[2];
		ctx->rdx = regs[3];

		cpu->hide_vm_exit_overhead = true;
		skip_instruction();
	}

	auto rdmsr(vcpu_t* cpu) -> void
	{
		if (cpu->ctx->ecx == IA32_FEATURE_CONTROL)
		{
			cpu->ctx->rax = cpu->cached.guest_feature_control.flags & 0xFFFF'FFFF;
			cpu->ctx->rdx = cpu->cached.guest_feature_control.flags >> 32;

			cpu->hide_vm_exit_overhead = true;
			skip_instruction();
			return;
		}

		host_exception_info e;

		auto const msr_value = rdmsr_safe(e, cpu->ctx->ecx);

		if (e.exception_occurred)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		cpu->ctx->rax = msr_value & 0xFFFF'FFFF;
		cpu->ctx->rdx = msr_value >> 32;

		cpu->hide_vm_exit_overhead = true;
		skip_instruction();
	}

	auto wrmsr(vcpu_t* cpu) -> void
	{
		auto const msr = cpu->ctx->ecx;
		auto const value = (cpu->ctx->rdx << 32) | cpu->ctx->eax;

		host_exception_info e;
		wrmsr_safe(e, msr, value);

		if (e.exception_occurred)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		cpu->hide_vm_exit_overhead = true;
		skip_instruction();
		return;
	}

	auto getsec(vcpu_t* cpu) -> void
	{
		inject_hw_exception(general_protection, 0);
	}

	auto invd(vcpu_t* cpu) -> void
	{
		inject_hw_exception(general_protection, 0);
	}

	auto xsetbv(vcpu_t* cpu) -> void
	{
		if (!read_effective_guest_cr4().os_xsave)
		{
			inject_hw_exception(invalid_opcode);
			return;
		}

		xcr0 new_xcr0;
		new_xcr0.flags = (cpu->ctx->rdx << 32) | cpu->ctx->eax;

		if (cpu->ctx->ecx != 0)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (new_xcr0.flags & cpu->cached.xcr0_unsupported_mask)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (!new_xcr0.x87)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (new_xcr0.avx && !new_xcr0.sse)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (!new_xcr0.avx && (new_xcr0.opmask || new_xcr0.zmm_hi256 || new_xcr0.zmm_hi16))
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (new_xcr0.bndreg != new_xcr0.bndcsr)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (new_xcr0.opmask != new_xcr0.zmm_hi256 || new_xcr0.zmm_hi256 != new_xcr0.zmm_hi16) 
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		host_exception_info e;
		xsetbv_safe(e, cpu->ctx->ecx, new_xcr0.flags);

		if (e.exception_occurred)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		cpu->hide_vm_exit_overhead = true;
		skip_instruction();
	}

	auto vmxon(vcpu_t* cpu) -> void
	{
		if (!read_effective_guest_cr4().vmx_enable)
		{
			inject_hw_exception(invalid_opcode);
			return;
		}

		inject_hw_exception(general_protection, 0);
	}

	auto vmcall(vcpu_t* cpu) -> void
	{
		auto const code = cpu->ctx->rax & 0xFF;
		auto const key = cpu->ctx->rax >> 8;

		if (key != hypercalls::hv_key)
		{
			inject_hw_exception(invalid_opcode);
			return;
		}

		switch (code)
		{
			case hypercalls::hypercall_ping:                  hypercalls::ping(cpu);                 return;
			case hypercalls::hypercall_hv_base:               hypercalls::hv_base(cpu);              return;
			case hypercalls::hypercall_hide_physical_page:    hypercalls::hide_physical_page(cpu);   return;
			case hypercalls::hypercall_instal_ept_hook:       hypercalls::install_ept_hook(cpu);     return;
			case hypercalls::hypercall_remove_ept_hook:       hypercalls::remove_ept_hook(cpu);      return;
			case hypercalls::hypercall_current_dirbase:       hypercalls::current_dirbase(cpu);      return;
			case hypercalls::hypercall_copy_virtual_memory:   hypercalls::copy_memory(cpu);          return;
		}

		inject_hw_exception(invalid_opcode);
	}

	auto vmx_preemption(vcpu_t* cpu) -> void
	{
		//do nothing
	}

	auto mov_to_cr0(vcpu_t* cpu, u64 gpr) -> void
	{
		cr0 new_cr0;
		new_cr0.flags = read_guest_gpr(cpu->ctx, gpr);

		auto const curr_cr0 = read_effective_guest_cr0();
		auto const curr_cr4 = read_effective_guest_cr4();

		new_cr0.reserved1 = 0;
		new_cr0.reserved2 = 0;
		new_cr0.reserved3 = 0;
		new_cr0.extension_type = 1;

		if (new_cr0.reserved4) {
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (new_cr0.paging_enable && !new_cr0.protection_enable)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (!new_cr0.cache_disable && new_cr0.not_write_through)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (!new_cr0.paging_enable)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (!new_cr0.write_protect && curr_cr4.control_flow_enforcement_enable)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		vm_write(VMCS_CTRL_CR0_READ_SHADOW, new_cr0.flags);

		new_cr0.flags |= cpu->cached.vmx_cr0_fixed0;
		new_cr0.flags &= cpu->cached.vmx_cr0_fixed1;

		vm_write(VMCS_GUEST_CR0, new_cr0.flags);

		cpu->hide_vm_exit_overhead = true;
		skip_instruction();
	}

	auto mov_to_cr3(vcpu_t* cpu, u64 gpr) -> void
	{
		cr3 new_cr3;
		new_cr3.flags = read_guest_gpr(cpu->ctx, gpr);

		auto const curr_cr4 = read_effective_guest_cr4();

		bool invalidate_tlb = true;

		if (curr_cr4.pcid_enable && (new_cr3.flags & (1ull << 63)))
		{
			invalidate_tlb = false;
			new_cr3.flags &= ~(1ull << 63);
		}

		auto const reserved_mask = ~((1ull << cpu->cached.max_phys_addr) - 1);

		if (new_cr3.flags & reserved_mask)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (invalidate_tlb)
		{
			invvpid_descriptor desc;
			desc.linear_address = 0;
			desc.reserved1 = 0;
			desc.reserved2 = 0;
			desc.vpid = 1;
			invvpid(invvpid_single_context_retaining_globals, desc);
		}

		vm_write(VMCS_GUEST_CR3, new_cr3.flags);

		cpu->hide_vm_exit_overhead = true;
		skip_instruction();
	}

	auto mov_to_cr4(vcpu_t* cpu, u64 gpr) -> void
	{
		cr4 new_cr4;
		new_cr4.flags = read_guest_gpr(cpu->ctx, gpr);

		cr3 curr_cr3;
		curr_cr3.flags = vm_read(VMCS_GUEST_CR3);

		auto const curr_cr0 = read_effective_guest_cr0();
		auto const curr_cr4 = read_effective_guest_cr4();

		if (!cpu->cached.cpuid_01.cpuid_feature_information_ecx.safer_mode_extensions
			&& new_cr4.smx_enable)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (new_cr4.reserved1 || new_cr4.reserved2)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if ((new_cr4.pcid_enable && !curr_cr4.pcid_enable) && (curr_cr3.flags & 0xFFF))
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (!new_cr4.physical_address_extension) {
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (new_cr4.linear_addresses_57_bit)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (new_cr4.control_flow_enforcement_enable && !curr_cr0.write_protect)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (new_cr4.page_global_enable != curr_cr4.page_global_enable ||
			!new_cr4.pcid_enable && curr_cr4.pcid_enable ||
			new_cr4.smep_enable && !curr_cr4.smep_enable)
		{
			invvpid_descriptor desc;
			desc.linear_address = 0;
			desc.reserved1 = 0;
			desc.reserved2 = 0;
			desc.vpid = 1;
			invvpid(invvpid_single_context, desc);
		}

		vm_write(VMCS_CTRL_CR4_READ_SHADOW, new_cr4.flags);

		new_cr4.flags |= cpu->cached.vmx_cr4_fixed0;
		new_cr4.flags &= cpu->cached.vmx_cr4_fixed1;

		vm_write(VMCS_GUEST_CR4, new_cr4.flags);

		cpu->hide_vm_exit_overhead = true;
		skip_instruction();
	}

	auto mov_from_cr3(vcpu_t* cpu, u64 gpr) -> void
	{
		write_guest_gpr(cpu->ctx, gpr, vm_read(VMCS_GUEST_CR3));

		cpu->hide_vm_exit_overhead = true;
		skip_instruction();
	}

	auto clts(vcpu_t* cpu) -> void
	{
		vm_write(VMCS_CTRL_CR0_READ_SHADOW,
			vm_read(VMCS_CTRL_CR0_READ_SHADOW) & ~CR0_TASK_SWITCHED_FLAG);

		vm_write(VMCS_GUEST_CR0, vm_read(VMCS_GUEST_CR0) & ~CR0_TASK_SWITCHED_FLAG);

		cpu->hide_vm_exit_overhead = true;
		skip_instruction();
	}

	auto lmsw(vcpu_t* cpu, u16 value) -> void
	{
		cr0 new_cr0;
		new_cr0.flags = value;

		cr0 shadow_cr0;
		shadow_cr0.flags = vm_read(VMCS_CTRL_CR0_READ_SHADOW);
		shadow_cr0.protection_enable = new_cr0.protection_enable;
		shadow_cr0.monitor_coprocessor = new_cr0.monitor_coprocessor;
		shadow_cr0.emulate_fpu = new_cr0.emulate_fpu;
		shadow_cr0.task_switched = new_cr0.task_switched;
		vm_write(VMCS_CTRL_CR0_READ_SHADOW, shadow_cr0.flags);

		cr0 real_cr0;
		real_cr0.flags = vm_read(VMCS_GUEST_CR0);
		real_cr0.protection_enable = new_cr0.protection_enable;
		real_cr0.monitor_coprocessor = new_cr0.monitor_coprocessor;
		real_cr0.emulate_fpu = new_cr0.emulate_fpu;
		real_cr0.task_switched = new_cr0.task_switched;
		vm_write(VMCS_GUEST_CR0, real_cr0.flags);

		cpu->hide_vm_exit_overhead = true;
		skip_instruction();
	}

	auto mov_cr(vcpu_t* cpu) -> void
	{
		vmx_exit_qualification_mov_cr qualification;
		qualification.flags = vm_read(VMCS_EXIT_QUALIFICATION);

		switch (qualification.access_type)
		{
		case VMX_EXIT_QUALIFICATION_ACCESS_MOV_TO_CR:
			switch (qualification.control_register) {
			case VMX_EXIT_QUALIFICATION_REGISTER_CR0:
				mov_to_cr0(cpu, qualification.general_purpose_register);
				break;
			case VMX_EXIT_QUALIFICATION_REGISTER_CR3:
				mov_to_cr3(cpu, qualification.general_purpose_register);
				break;
			case VMX_EXIT_QUALIFICATION_REGISTER_CR4:
				mov_to_cr4(cpu, qualification.general_purpose_register);
				break;
			}
			break;
		case VMX_EXIT_QUALIFICATION_ACCESS_MOV_FROM_CR:
			mov_from_cr3(cpu, qualification.general_purpose_register);
			break;
		case VMX_EXIT_QUALIFICATION_ACCESS_CLTS:
			clts(cpu);
			break;
		case VMX_EXIT_QUALIFICATION_ACCESS_LMSW:
			lmsw(cpu, qualification.lmsw_source_data);
			break;
		}
	}

	auto nmi_window(vcpu_t* cpu) -> void
	{
		--cpu->queued_nmis;

		inject_nmi();

		if (cpu->queued_nmis == 0)
		{
			auto ctrl = read_ctrl_proc_based();
			ctrl.nmi_window_exiting = 0;
			write_ctrl_proc_based(ctrl);
		}

		if (cpu->queued_nmis > 0)
		{
			auto ctrl = read_ctrl_proc_based();
			ctrl.nmi_window_exiting = 1;
			write_ctrl_proc_based(ctrl);
		}
	}

	auto exception_or_nmi(vcpu_t* cpu) -> void
	{
		++cpu->queued_nmis;

		auto ctrl = read_ctrl_proc_based();
		ctrl.nmi_window_exiting = 1;
		write_ctrl_proc_based(ctrl);
	}

	auto vmx_instruction(vcpu_t* cpu) -> void
	{
		inject_hw_exception(invalid_opcode);
	}

	auto rdtsc(vcpu_t* cpu) -> void
	{
		auto const tsc = __rdtsc();

		cpu->ctx->rax = tsc & 0xFFFFFFFF;
		cpu->ctx->rdx = (tsc >> 32) & 0xFFFFFFFF;

		skip_instruction();
	}

	auto rdtscp(vcpu_t* cpu) -> void
	{
		unsigned int aux = 0;
		auto const tsc = __rdtscp(&aux);

		cpu->ctx->rax = tsc & 0xFFFFFFFF;
		cpu->ctx->rdx = (tsc >> 32) & 0xFFFFFFFF;
		cpu->ctx->rcx = aux;

		skip_instruction();
	}

	auto handlers::ept_violation(vcpu_t* cpu) -> void
	{
		vmx_exit_qualification_ept_violation qualification;
		qualification.flags = vm_read(VMCS_EXIT_QUALIFICATION);

		if (!qualification.caused_by_translation)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		auto physical_address = PAGE_ALIGN(vm_read(VMCS_GUEST_PHYSICAL_ADDRESS));

		auto* hook = ghv.ept->find_ept_hook(physical_address);

		if (!hook)
		{
			inject_hw_exception(general_protection, 0);
			return;
		}

		if (qualification.execute_access)
		{
			hook->target_page->flags = hook->exec_page.flags;
		}

		if (qualification.read_access || qualification.write_access)
		{
			hook->target_page->flags = hook->rw_page.flags;
		}
	}

	auto handlers::ept_misconfiguration(vcpu_t* cpu) -> void
	{
		//do notinh
	}
}

