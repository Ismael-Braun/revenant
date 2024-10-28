#include "vmx.h"

namespace vmx
{
	auto vmx_on(u64 vmxon_phys_addr) -> bool
	{
		return __vmx_on(&vmxon_phys_addr) == 0;
	}

	auto vmx_off() -> void
	{
		__vmx_off();
	}

	auto vm_clear(u64 vmcs_phys_addr) -> bool
	{
		return __vmx_vmclear(&vmcs_phys_addr) == 0;
	}

	auto vm_ptrld(u64 vmcs_phys_addr) -> bool
	{
		return __vmx_vmptrld(&vmcs_phys_addr) == 0;
	}

	auto vm_write(u64 field, u64 value) -> void
	{
		__vmx_vmwrite(field, value);
	}

	auto vm_read(u64 field) -> u64
	{
		uint64_t value;
		__vmx_vmread(field, &value);
		return value;
	}

	auto find_system_directory_table_base()->uptr
	{
		struct NT_KPROCESS
		{
			DISPATCHER_HEADER header;
			LIST_ENTRY profile_list_head;
			ULONG_PTR directory_table_base;
			UCHAR data[1];
		};

		NT_KPROCESS* system_process = reinterpret_cast<NT_KPROCESS*>(PsInitialSystemProcess);
		return system_process->directory_table_base;
	}

	auto get_system_cr3(uptr* system_cr3) -> void
	{
		uptr system_directory_table_base = find_system_directory_table_base();

		*system_cr3 = system_directory_table_base;
	}

	auto set_ctrl_field(size_t value, u64 ctrl_field, u64 cap_msr, u64 true_cap_msr) -> void
	{
		ia32_vmx_basic_register vmx_basic;
		vmx_basic.flags = __readmsr(IA32_VMX_BASIC);

		auto const cap = __readmsr(vmx_basic.vmx_controls ? true_cap_msr : cap_msr);

		value &= cap >> 32;
		value |= cap & 0xFFFFFFFF;

		vm_write(ctrl_field, value);
	}

	auto pin_based_ctrls(pinbased_ctls_t value) -> void
	{
		set_ctrl_field(value.flags,
			VMCS_CTRL_PIN_BASED_VM_EXECUTION_CONTROLS,
			IA32_VMX_PINBASED_CTLS,
			IA32_VMX_TRUE_PINBASED_CTLS);
	}

	auto proc_based_ctrls(procbased_ctls_t value) -> void
	{
		set_ctrl_field(value.flags,
			VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,
			IA32_VMX_PROCBASED_CTLS,
			IA32_VMX_TRUE_PROCBASED_CTLS);
	}

	auto proc_based2_ctrls(procbased2_ctls_t value) -> void
	{
		set_ctrl_field(value.flags,
			VMCS_CTRL_SECONDARY_PROCESSOR_BASED_VM_EXECUTION_CONTROLS,
			IA32_VMX_PROCBASED_CTLS2,
			IA32_VMX_PROCBASED_CTLS2);
	}

	auto exit_ctrls(exit_ctls_t value) -> void
	{
		set_ctrl_field(value.flags,
			VMCS_CTRL_PRIMARY_VMEXIT_CONTROLS,
			IA32_VMX_EXIT_CTLS,
			IA32_VMX_TRUE_EXIT_CTLS);
	}

	auto entry_ctrls(entry_ctls_t value) -> void
	{
		set_ctrl_field(value.flags,
			VMCS_CTRL_VMENTRY_CONTROLS,
			IA32_VMX_ENTRY_CTLS,
			IA32_VMX_TRUE_ENTRY_CTLS);
	}

	auto read_interrupt_state() -> interrupt_state_t
	{
		interrupt_state_t value;
		value.flags = static_cast<u32>(vm_read(VMCS_GUEST_INTERRUPTIBILITY_STATE));
		return value;
	}

	auto write_interrupt_state(interrupt_state_t value) -> void
	{
		vm_write(VMCS_GUEST_INTERRUPTIBILITY_STATE, value.flags);
	}

	auto skip_instruction() -> void
	{
		auto const old_rip = vm_read(VMCS_GUEST_RIP);
		auto new_rip = old_rip + vm_read(VMCS_VMEXIT_INSTRUCTION_LENGTH);

		if (old_rip < (1ull << 32) && new_rip >= (1ull << 32)) {
			vmx_segment_access_rights cs_access_rights;
			cs_access_rights.flags = static_cast<u32>(
				vm_read(VMCS_GUEST_CS_ACCESS_RIGHTS));


			if (!cs_access_rights.long_mode)
				new_rip &= 0xFFFF'FFFF;
		}
		
		vm_write(VMCS_GUEST_RIP, new_rip);

		auto interrupt_state = read_interrupt_state();
		interrupt_state.blocking_by_mov_ss = 0;
		interrupt_state.blocking_by_sti = 0;
		write_interrupt_state(interrupt_state);

		ia32_debugctl_register debugctl;
		debugctl.flags = vm_read(VMCS_GUEST_DEBUGCTL);

		rflags rflags;
		rflags.flags = vm_read(VMCS_GUEST_RFLAGS);

		if (rflags.trap_flag && !debugctl.btf) {
			vmx_pending_debug_exceptions dbg_exception;
			dbg_exception.flags = vm_read(VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS);
			dbg_exception.bs = 1;
			vm_write(VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, dbg_exception.flags);
		}
	}

	auto inject_hw_exception(u32 vector) -> void
	{
		vmentry_interrupt_information interrupt_info;
		interrupt_info.flags = 0;
		interrupt_info.vector = vector;
		interrupt_info.interruption_type = hardware_exception;
		interrupt_info.deliver_error_code = 0;
		interrupt_info.valid = 1;
		vm_write(VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, interrupt_info.flags);
	}

	auto inject_hw_exception(u32 vector, u32 error) -> void
	{
		vmentry_interrupt_information interrupt_info;
		interrupt_info.flags = 0;
		interrupt_info.vector = vector;
		interrupt_info.interruption_type = hardware_exception;
		interrupt_info.deliver_error_code = 1;
		interrupt_info.valid = 1;
		vm_write(VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, interrupt_info.flags);
		vm_write(VMCS_CTRL_VMENTRY_EXCEPTION_ERROR_CODE, error);
	}

	PMDL lock_pages(void* virtual_address, LOCK_OPERATION operation, int size)
	{
		PMDL mdl = IoAllocateMdl(virtual_address, size, FALSE, FALSE, nullptr);

		MmProbeAndLockPages(mdl, KernelMode, operation);

		return mdl;
	}

	NTSTATUS unlock_pages(PMDL mdl)
	{
		MmUnlockPages(mdl);
		IoFreeMdl(mdl);

		return STATUS_SUCCESS;
	}

	u16 current_guest_cpl()
	{
		vmx_segment_access_rights ss;
		ss.flags = static_cast<uint32_t>(vm_read(VMCS_GUEST_SS_ACCESS_RIGHTS));
		return ss.descriptor_privilege_level;
	}

	cr4 read_effective_guest_cr4()
	{
		auto const mask = vm_read(VMCS_CTRL_CR4_GUEST_HOST_MASK);

		cr4 cr4;
		cr4.flags = (vm_read(VMCS_CTRL_CR4_READ_SHADOW) & mask)
			| (vm_read(VMCS_GUEST_CR4) & ~mask);

		return cr4;
	}

	cr0 read_effective_guest_cr0()
	{
		auto const mask = vm_read(VMCS_CTRL_CR0_GUEST_HOST_MASK);

		cr0 cr0;
		cr0.flags = (vm_read(VMCS_CTRL_CR0_READ_SHADOW) & mask)
			| (vm_read(VMCS_GUEST_CR0) & ~mask);

		return cr0;
	}

	void write_guest_gpr(guest_registers* ctx, u64 gpr_idx, u64 value)
	{
		if (gpr_idx == VMX_EXIT_QUALIFICATION_GENREG_RSP)
			vm_write(VMCS_GUEST_RSP, value);
		else
			ctx->gpr[gpr_idx] = value;
	}

	u64 read_guest_gpr(guest_registers* ctx, u64 gpr_idx)
	{
		if (gpr_idx == VMX_EXIT_QUALIFICATION_GENREG_RSP)
			return vm_read(VMCS_GUEST_RSP);
		return ctx->gpr[gpr_idx];
	}

	ia32_vmx_procbased_ctls_register read_ctrl_proc_based()
	{
		ia32_vmx_procbased_ctls_register value;
		value.flags = vm_read(VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS);
		return value;
	}

	void write_ctrl_proc_based(ia32_vmx_procbased_ctls_register const value)
	{
		vm_write(VMCS_CTRL_PROCESSOR_BASED_VM_EXECUTION_CONTROLS, value.flags);
	}

	segment_descriptor_interrupt_gate_64 create_interrupt_gate(void* handler, segment_selector cs_selector)
	{
		segment_descriptor_interrupt_gate_64 gate;

		gate.interrupt_stack_table = 0;
		gate.segment_selector = cs_selector.flags;
		gate.must_be_zero_0 = 0;
		gate.type = SEGMENT_DESCRIPTOR_TYPE_INTERRUPT_GATE;
		gate.must_be_zero_1 = 0;
		gate.descriptor_privilege_level = 0;
		gate.present = 1;
		gate.reserved = 0;

		auto const offset = reinterpret_cast<uint64_t>(handler);
		gate.offset_low = (offset >> 0) & 0xFFFF;
		gate.offset_middle = (offset >> 16) & 0xFFFF;
		gate.offset_high = (offset >> 32) & 0xFFFFFFFF;

		return gate;
	}

	void inject_nmi()
	{
		vmentry_interrupt_information interrupt_info;
		interrupt_info.flags = 0;
		interrupt_info.vector = nmi;
		interrupt_info.interruption_type = non_maskable_interrupt;
		interrupt_info.deliver_error_code = 0;
		interrupt_info.valid = 1;
		vm_write(VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, interrupt_info.flags);
	}

	void enable_exit_for_msr_read(vmx_msr_bitmap& bitmap, uint32_t const msr, bool const enable_exiting)
	{
		auto const bit = static_cast<uint8_t>(enable_exiting ? 1 : 0);

		if (msr <= MSR_ID_LOW_MAX)
			// set the bit in the low bitmap
			bitmap.rdmsr_low[msr / 8] = (bit << (msr & 0b0111));
		else if (msr >= MSR_ID_HIGH_MIN && msr <= MSR_ID_HIGH_MAX)
			// set the bit in the high bitmap
			bitmap.rdmsr_high[(msr - MSR_ID_HIGH_MIN) / 8] = (bit << (msr & 0b0111));
	}

	void enable_exit_for_msr_write(vmx_msr_bitmap& bitmap,
		uint32_t const msr, bool const enable_exiting) {
		auto const bit = static_cast<uint8_t>(enable_exiting ? 1 : 0);

		if (msr <= MSR_ID_LOW_MAX)
			// set the bit in the low bitmap
			bitmap.wrmsr_low[msr / 8] = (bit << (msr & 0b0111));
		else if (msr >= MSR_ID_HIGH_MIN && msr <= MSR_ID_HIGH_MAX)
			// set the bit in the high bitmap
			bitmap.wrmsr_high[(msr - MSR_ID_HIGH_MIN) / 8] = (bit << (msr & 0b0111));
	}
}