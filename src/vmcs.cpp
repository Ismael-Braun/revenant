#include "vmcs.h"
#include "hv.h"
#include "vmx.h"
#include "segment.h"
#include "timing.h"
#include "vmexit.h"

using namespace vmx;

namespace hv
{

	auto setup_vmcs_ctrl(vcpu_t* cpu) -> void
	{
		auto pin_based = pinbased_ctls_t{};
		pin_based.flags = 0;
		pin_based.virtual_nmi = 1;
		pin_based.nmi_exiting = 1;
		pin_based.activate_vmx_preemption_timer = 1;
		pin_based_ctrls(pin_based);

		auto proc_based = procbased_ctls_t{};
		proc_based.flags = 0;
		proc_based.cr3_load_exiting = 1;
		proc_based.use_msr_bitmaps = 1;
		proc_based.use_tsc_offsetting = 1;
		proc_based.activate_secondary_controls = 1;
		proc_based_ctrls(proc_based);

		auto proc_based2 = procbased2_ctls_t{};
		proc_based2.flags = 0;
		proc_based2.enable_ept = 1;
		proc_based2.enable_rdtscp = 1;
		proc_based2.enable_vpid = 1;
		proc_based2.enable_invpcid = 1;
		proc_based2.enable_xsaves = 1;
		proc_based2.enable_user_wait_pause = 1;
		proc_based2.conceal_vmx_from_pt = 1;
		proc_based2_ctrls(proc_based2);

		auto exit_ctrl = exit_ctls_t{};
		exit_ctrl.flags = 0;
		exit_ctrl.save_debug_controls = 1;
		exit_ctrl.host_address_space_size = 1;
		exit_ctrl.save_ia32_pat = 1;
		exit_ctrl.load_ia32_pat = 1;
		exit_ctrl.load_ia32_perf_global_ctrl = 1;
		exit_ctrl.conceal_vmx_from_pt = 1;
		exit_ctrls(exit_ctrl);

		auto entry_ctrl = entry_ctls_t{};
		entry_ctrl.flags = 0;
		entry_ctrl.load_debug_controls = 1;
		entry_ctrl.ia32e_mode_guest = 1;
		entry_ctrl.load_ia32_pat = 1;
		entry_ctrl.load_ia32_perf_global_ctrl = 1;
		entry_ctrl.conceal_vmx_from_pt = 1;
		entry_ctrls(entry_ctrl);

		vm_write(VMCS_CTRL_EPT_POINTER, ghv.ept->get_ept_pointer().flags);

		vm_write(VMCS_CTRL_CR0_GUEST_HOST_MASK, cpu->cached.vmx_cr0_fixed0 | ~cpu->cached.vmx_cr0_fixed1 |
			CR0_CACHE_DISABLE_FLAG | CR0_WRITE_PROTECT_FLAG);

		vm_write(VMCS_CTRL_CR4_GUEST_HOST_MASK, cpu->cached.vmx_cr4_fixed0 | ~cpu->cached.vmx_cr4_fixed1);

		vm_write(VMCS_CTRL_CR0_READ_SHADOW, __readcr0());
		vm_write(VMCS_CTRL_CR4_READ_SHADOW, __readcr4() & ~CR4_VMX_ENABLE_FLAG);

		vm_write(VMCS_CTRL_CR3_TARGET_COUNT, 1);
		vm_write(VMCS_CTRL_CR3_TARGET_VALUE_0, ghv.system_cr3.flags);

		vm_write(VMCS_CTRL_MSR_BITMAP_ADDRESS, MmGetPhysicalAddress(&cpu->msr_bitmap).QuadPart);

		vm_write(VMCS_CTRL_VIRTUAL_PROCESSOR_IDENTIFIER, 1);

		cpu->msr_exit_store.tsc.msr_idx = IA32_TIME_STAMP_COUNTER;
		cpu->msr_exit_store.perf_global_ctrl.msr_idx = IA32_PERF_GLOBAL_CTRL;
		cpu->msr_exit_store.aperf.msr_idx = IA32_APERF;
		cpu->msr_exit_store.mperf.msr_idx = IA32_MPERF;

		vm_write(VMCS_CTRL_VMEXIT_MSR_STORE_COUNT, sizeof(cpu->msr_exit_store) / 16);
		vm_write(VMCS_CTRL_VMEXIT_MSR_STORE_ADDRESS, MmGetPhysicalAddress(&cpu->msr_exit_store).QuadPart);

		cpu->msr_entry_load.aperf.msr_idx = IA32_APERF;
		cpu->msr_entry_load.mperf.msr_idx = IA32_MPERF;
		cpu->msr_entry_load.aperf.msr_data = __readmsr(IA32_APERF);
		cpu->msr_entry_load.mperf.msr_data = __readmsr(IA32_MPERF);

		vm_write(VMCS_CTRL_VMENTRY_MSR_LOAD_COUNT, sizeof(cpu->msr_entry_load) / 16);
		vm_write(VMCS_CTRL_VMENTRY_MSR_LOAD_ADDRESS, MmGetPhysicalAddress(&cpu->msr_entry_load).QuadPart);

		vm_write(VMCS_CTRL_VMENTRY_INTERRUPTION_INFORMATION_FIELD, 0);
		vm_write(VMCS_CTRL_VMENTRY_EXCEPTION_ERROR_CODE, 0);
		vm_write(VMCS_CTRL_VMENTRY_INSTRUCTION_LENGTH, 0);
		vm_write(VMCS_CTRL_EXCEPTION_BITMAP, 0);
		vm_write(VMCS_CTRL_PAGEFAULT_ERROR_CODE_MASK, 0);
		vm_write(VMCS_CTRL_PAGEFAULT_ERROR_CODE_MATCH, 0);
		vm_write(VMCS_CTRL_TSC_OFFSET, 0);
		vm_write(VMCS_CTRL_VMEXIT_MSR_LOAD_COUNT, 0);
		vm_write(VMCS_CTRL_VMEXIT_MSR_LOAD_ADDRESS, 0);
	}

	auto setup_vmcs_host(vcpu_t const* cpu) -> void
	{
		vm_write(VMCS_HOST_CR3, ghv.page_table_cr3.flags);

		cr4 host_cr4;
		host_cr4.flags = __readcr4();
		host_cr4.fsgsbase_enable = 1;
		host_cr4.os_xsave = 1;
		host_cr4.smap_enable = 0;
		host_cr4.smep_enable = 0;

		vm_write(VMCS_HOST_CR0, __readcr0());
		vm_write(VMCS_HOST_CR4, host_cr4.flags);

		auto const rsp = ((reinterpret_cast<size_t>(cpu->host_stack)
			+ 0x6000) & ~0b1111ull) - 8;

		vm_write(VMCS_HOST_RSP, rsp);
		vm_write(VMCS_HOST_RIP, reinterpret_cast<size_t>(vm_exit));

		vm_write(VMCS_HOST_CS_SELECTOR, host_cs_selector.flags);
		vm_write(VMCS_HOST_SS_SELECTOR, 0x00);
		vm_write(VMCS_HOST_DS_SELECTOR, 0x00);
		vm_write(VMCS_HOST_ES_SELECTOR, 0x00);
		vm_write(VMCS_HOST_FS_SELECTOR, 0x00);
		vm_write(VMCS_HOST_GS_SELECTOR, 0x00);
		vm_write(VMCS_HOST_TR_SELECTOR, host_tr_selector.flags);

		vm_write(VMCS_HOST_FS_BASE, reinterpret_cast<size_t>(cpu));
		vm_write(VMCS_HOST_GS_BASE, 0);
		vm_write(VMCS_HOST_TR_BASE, reinterpret_cast<size_t>(&cpu->host_tss));
		vm_write(VMCS_HOST_GDTR_BASE, reinterpret_cast<size_t>(&cpu->host_gdt));
		vm_write(VMCS_HOST_IDTR_BASE, reinterpret_cast<size_t>(&cpu->host_idt));

		vm_write(VMCS_HOST_SYSENTER_CS, 0);
		vm_write(VMCS_HOST_SYSENTER_ESP, 0);
		vm_write(VMCS_HOST_SYSENTER_EIP, 0);

		ia32_pat_register host_pat;
		host_pat.flags = 0;
		host_pat.pa0 = MEMORY_TYPE_WRITE_BACK;
		host_pat.pa1 = MEMORY_TYPE_WRITE_THROUGH;
		host_pat.pa2 = MEMORY_TYPE_UNCACHEABLE_MINUS;
		host_pat.pa3 = MEMORY_TYPE_UNCACHEABLE;
		host_pat.pa4 = MEMORY_TYPE_WRITE_BACK;
		host_pat.pa5 = MEMORY_TYPE_WRITE_THROUGH;
		host_pat.pa6 = MEMORY_TYPE_UNCACHEABLE_MINUS;
		host_pat.pa7 = MEMORY_TYPE_UNCACHEABLE;
		vm_write(VMCS_HOST_PAT, host_pat.flags);

		vm_write(VMCS_HOST_PERF_GLOBAL_CTRL, 0);
	}

	auto setup_vmcs_guest() -> void
	{
		vm_write(VMCS_GUEST_CR3, __readcr3());

		vm_write(VMCS_GUEST_CR0, __readcr0());
		vm_write(VMCS_GUEST_CR4, __readcr4());

		vm_write(VMCS_GUEST_DR7, __readdr(7));

		// RIP and RSP are set in vm-launch.asm
		vm_write(VMCS_GUEST_RSP, 0);
		vm_write(VMCS_GUEST_RIP, 0);

		vm_write(VMCS_GUEST_RFLAGS, __readeflags());

		vm_write(VMCS_GUEST_CS_SELECTOR, read_cs().flags);
		vm_write(VMCS_GUEST_SS_SELECTOR, read_ss().flags);
		vm_write(VMCS_GUEST_DS_SELECTOR, read_ds().flags);
		vm_write(VMCS_GUEST_ES_SELECTOR, read_es().flags);
		vm_write(VMCS_GUEST_FS_SELECTOR, read_fs().flags);
		vm_write(VMCS_GUEST_GS_SELECTOR, read_gs().flags);
		vm_write(VMCS_GUEST_TR_SELECTOR, read_tr().flags);
		vm_write(VMCS_GUEST_LDTR_SELECTOR, read_ldtr().flags);

		segment_descriptor_register_64 gdtr, idtr;
		_sgdt(&gdtr);
		__sidt(&idtr);

		vm_write(VMCS_GUEST_CS_BASE, segment_base(gdtr, read_cs()));
		vm_write(VMCS_GUEST_SS_BASE, segment_base(gdtr, read_ss()));
		vm_write(VMCS_GUEST_DS_BASE, segment_base(gdtr, read_ds()));
		vm_write(VMCS_GUEST_ES_BASE, segment_base(gdtr, read_es()));
		vm_write(VMCS_GUEST_FS_BASE, __readmsr(IA32_FS_BASE));
		vm_write(VMCS_GUEST_GS_BASE, __readmsr(IA32_GS_BASE));
		vm_write(VMCS_GUEST_TR_BASE, segment_base(gdtr, read_tr()));
		vm_write(VMCS_GUEST_LDTR_BASE, segment_base(gdtr, read_ldtr()));

		vm_write(VMCS_GUEST_CS_LIMIT, __segmentlimit(read_cs().flags));
		vm_write(VMCS_GUEST_SS_LIMIT, __segmentlimit(read_ss().flags));
		vm_write(VMCS_GUEST_DS_LIMIT, __segmentlimit(read_ds().flags));
		vm_write(VMCS_GUEST_ES_LIMIT, __segmentlimit(read_es().flags));
		vm_write(VMCS_GUEST_FS_LIMIT, __segmentlimit(read_fs().flags));
		vm_write(VMCS_GUEST_GS_LIMIT, __segmentlimit(read_gs().flags));
		vm_write(VMCS_GUEST_TR_LIMIT, __segmentlimit(read_tr().flags));
		vm_write(VMCS_GUEST_LDTR_LIMIT, __segmentlimit(read_ldtr().flags));

		vm_write(VMCS_GUEST_CS_ACCESS_RIGHTS, segment_access(gdtr, read_cs()).flags);
		vm_write(VMCS_GUEST_SS_ACCESS_RIGHTS, segment_access(gdtr, read_ss()).flags);
		vm_write(VMCS_GUEST_DS_ACCESS_RIGHTS, segment_access(gdtr, read_ds()).flags);
		vm_write(VMCS_GUEST_ES_ACCESS_RIGHTS, segment_access(gdtr, read_es()).flags);
		vm_write(VMCS_GUEST_FS_ACCESS_RIGHTS, segment_access(gdtr, read_fs()).flags);
		vm_write(VMCS_GUEST_GS_ACCESS_RIGHTS, segment_access(gdtr, read_gs()).flags);
		vm_write(VMCS_GUEST_TR_ACCESS_RIGHTS, segment_access(gdtr, read_tr()).flags);
		vm_write(VMCS_GUEST_LDTR_ACCESS_RIGHTS, segment_access(gdtr, read_ldtr()).flags);

		vm_write(VMCS_GUEST_GDTR_BASE, gdtr.base_address);
		vm_write(VMCS_GUEST_IDTR_BASE, idtr.base_address);

		vm_write(VMCS_GUEST_GDTR_LIMIT, gdtr.limit);
		vm_write(VMCS_GUEST_IDTR_LIMIT, idtr.limit);

		vm_write(VMCS_GUEST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
		vm_write(VMCS_GUEST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));
		vm_write(VMCS_GUEST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));
		vm_write(VMCS_GUEST_DEBUGCTL, __readmsr(IA32_DEBUGCTL));
		vm_write(VMCS_GUEST_PAT, __readmsr(IA32_PAT));
		vm_write(VMCS_GUEST_PERF_GLOBAL_CTRL, __readmsr(IA32_PERF_GLOBAL_CTRL));

		vm_write(VMCS_GUEST_ACTIVITY_STATE, vmx_active);

		vm_write(VMCS_GUEST_INTERRUPTIBILITY_STATE, 0);

		vm_write(VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, 0);

		vm_write(VMCS_GUEST_VMCS_LINK_POINTER, MAXULONG64);

		vm_write(VMCS_GUEST_VMX_PREEMPTION_TIMER_VALUE, MAXULONG64);
	}
}

