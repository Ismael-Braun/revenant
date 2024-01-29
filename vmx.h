#pragma once

#include "types.h"
#include "guest_registers.h"

using namespace hv;

namespace vmx
{
	auto invept(invept_type type, invept_descriptor const& desc) -> void;
	auto invvpid(invvpid_type type, invvpid_descriptor const& desc) -> void;

	struct host_exception_info
	{
		bool exception_occurred;
		uint64_t vector;
		uint64_t error;
	};

	void memcpy_safe(host_exception_info& e, void* dst, void const* src, size_t size);
	void xsetbv_safe(host_exception_info& e, uint32_t idx, uint64_t value);
	void wrmsr_safe(host_exception_info& e, uint32_t msr, uint64_t value);
	uint64_t rdmsr_safe(host_exception_info& e, uint32_t msr);

	auto vmx_on(u64 vmxon_phys_addr) -> bool;
	auto vmx_off() -> void;
	auto vm_clear(u64 vmcs_phys_addr) -> bool;
	auto vm_ptrld(u64 vmcs_phys_addr) -> bool;
	auto vm_write(u64 field, u64 value) -> void;
	auto vm_read(u64 field)->u64;

	auto get_system_cr3(uptr* system_cr3) -> void;

	auto set_ctrl_field(size_t value, u64 ctrl_field, u64 cap_msr, u64 true_cap_msr) -> void;
	auto pin_based_ctrls(pinbased_ctls_t value) -> void;
	auto proc_based_ctrls(procbased_ctls_t value) -> void;
	auto proc_based2_ctrls(procbased2_ctls_t value) -> void;
	auto exit_ctrls(exit_ctls_t value) -> void;
	auto entry_ctrls(entry_ctls_t value) -> void;

	auto read_interrupt_state()->interrupt_state_t;
	auto write_interrupt_state(interrupt_state_t value) -> void;

	auto skip_instruction() -> void;

	auto inject_hw_exception(u32 vector) -> void;
	auto inject_hw_exception(u32 vector, u32 error) -> void;

	PMDL lock_pages(void* virtual_address, LOCK_OPERATION operation, int size = PAGE_SIZE);
	NTSTATUS unlock_pages(PMDL mdl);

	u16 current_guest_cpl();

	cr4 read_effective_guest_cr4();
	cr0 read_effective_guest_cr0();

	ia32_vmx_procbased_ctls_register read_ctrl_proc_based();
	void write_ctrl_proc_based(ia32_vmx_procbased_ctls_register const value);

	void write_guest_gpr(guest_registers* ctx, u64 gpr_idx, u64 value);
	u64 read_guest_gpr(guest_registers* ctx, u64 gpr_idx);

	segment_descriptor_interrupt_gate_64 create_interrupt_gate(void* handler, segment_selector cs_selector);

	void inject_nmi();

	void enable_exit_for_msr_read(vmx_msr_bitmap& bitmap, uint32_t const msr, bool const enable_exiting);
	void enable_exit_for_msr_write(vmx_msr_bitmap& bitmap, uint32_t const msr, bool const enable_exiting);
}

