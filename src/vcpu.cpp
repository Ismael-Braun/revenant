#include "vcpu.h"
#include "hv.h"
#include "gdt.h"
#include "idt.h"
#include "vmx.h"
#include "vmcs.h"
#include "timing.h"
#include "trap-frame.h"
#include "hypercalls.h"
#include "handlers.h"
#include "vmexit.h"

using namespace vmx;

namespace hv 
{
	auto virtualize_cpu(vcpu_t* cpu) -> bool
	{
		memset(cpu, 0, sizeof(*cpu));

		auto current_vcpu = KeGetCurrentProcessorNumber() + 1;

		if (!setup_vmx(cpu))
			return false;

		if (!setup_vmxon(cpu))
			return false;

		if (!setup_vmcs(cpu))
			return false;

		setup_external_structures(cpu);

		setup_vmcs_ctrl(cpu);
		setup_vmcs_host(cpu);
		setup_vmcs_guest();

		cpu->ctx = nullptr;
		cpu->queued_nmis = 0;
		cpu->tsc_offset = 0;
		cpu->preemption_timer = 0;
		cpu->vm_exit_tsc_overhead = 0;
		cpu->vm_exit_mperf_overhead = 0;
		cpu->vm_exit_ref_tsc_overhead = 0;

		if (!vm_launch())
		{
			log_error("VMLAUNCH failed. Instruction error = %lli", vm_read(VMCS_VM_INSTRUCTION_ERROR));

			vmx_off();
			return false;
		}

		cpu->vm_exit_tsc_overhead = measure_vm_exit_tsc_overhead();
		cpu->vm_exit_mperf_overhead = measure_vm_exit_mperf_overhead();
		cpu->vm_exit_ref_tsc_overhead = measure_vm_exit_ref_tsc_overhead();

		log_info("VM-exit overhead (TSC = %zi)", cpu->vm_exit_tsc_overhead);
		log_info("VM-exit overhead (MPERF = %zi)", cpu->vm_exit_mperf_overhead);
		log_info("VM-exit overhead (CPU_CLK_UNHALTED.REF_TSC = %zi)", cpu->vm_exit_ref_tsc_overhead);

		log_info("vcpu -> %d virtualized!", current_vcpu);

		return true;
	}

	auto cache_cpu_data(vcpu_cached_data& cached) -> void
	{
		__cpuid(reinterpret_cast<int*>(&cached.cpuid_01), 0x01);

		if (!cached.cpuid_01.cpuid_feature_information_ecx.virtual_machine_extensions)
			return;

		cpuid_eax_80000008 cpuid_80000008;
		__cpuid(reinterpret_cast<int*>(&cpuid_80000008), 0x80000008);

		cached.max_phys_addr = cpuid_80000008.eax.number_of_physical_address_bits;

		cached.vmx_cr0_fixed0 = __readmsr(IA32_VMX_CR0_FIXED0);
		cached.vmx_cr0_fixed1 = __readmsr(IA32_VMX_CR0_FIXED1);
		cached.vmx_cr4_fixed0 = __readmsr(IA32_VMX_CR4_FIXED0);
		cached.vmx_cr4_fixed1 = __readmsr(IA32_VMX_CR4_FIXED1);

		cpuid_eax_0d_ecx_00 cpuid_0d;
		__cpuidex(reinterpret_cast<int*>(&cpuid_0d), 0x0D, 0x00);

		cached.xcr0_unsupported_mask = ~((static_cast<uint64_t>(
			cpuid_0d.edx.flags) << 32) | cpuid_0d.eax.flags);

		cached.feature_control.flags = __readmsr(IA32_FEATURE_CONTROL);
		cached.vmx_misc.flags = __readmsr(IA32_VMX_MISC);

		cached.guest_feature_control = cached.feature_control;
		cached.guest_feature_control.lock_bit = 1;
		cached.guest_feature_control.enable_vmx_inside_smx = 0;
		cached.guest_feature_control.enable_vmx_outside_smx = 0;
		cached.guest_feature_control.senter_local_function_enables = 0;
		cached.guest_feature_control.senter_global_enable = 0;
	}

	auto setup_vmx(vcpu_t* cpu) -> bool
	{
		cache_cpu_data(cpu->cached);

		if (!cpu->cached.cpuid_01.cpuid_feature_information_ecx.virtual_machine_extensions)
		{
			log_error("vmx not supported!");
			return false;
		}

		if (!cpu->cached.feature_control.lock_bit || !cpu->cached.feature_control.enable_vmx_outside_smx)
		{
			log_error("vmx not enabled outside smx");
			return false;
		}

		_disable();

		auto cr0 = __readcr0();
		auto cr4 = __readcr4();

		cr4 |= CR4_VMX_ENABLE_FLAG;

		cr0 |= __readmsr(IA32_VMX_CR0_FIXED0);
		cr0 &= __readmsr(IA32_VMX_CR0_FIXED1);
		cr4 |= __readmsr(IA32_VMX_CR4_FIXED0);
		cr4 &= __readmsr(IA32_VMX_CR4_FIXED1);

		__writecr0(cr0);
		__writecr4(cr4);

		_enable();

		return true;
	}

	auto setup_vmcs(vcpu_t* cpu) -> bool
	{
		ia32_vmx_basic_register vmx_basic;
		vmx_basic.flags = __readmsr(IA32_VMX_BASIC);

		cpu->vmcs.revision_id = vmx_basic.vmcs_revision_id;
		cpu->vmcs.shadow_vmcs_indicator = 0;

		auto vmcs_phys = MmGetPhysicalAddress(&cpu->vmcs).QuadPart;
		NT_ASSERT(vmcs_phys % 0x1000 == 0);

		if (!vm_clear(vmcs_phys))
		{
			log_error("vmxclear error");
			return false;
		}

		if (!vm_ptrld(vmcs_phys))
		{
			log_error("vmptrld error");
			return false;
		}

		log_info("vmcs region phys created at -> %p", vmcs_phys);

		return true;
	}

	auto setup_vmxon(vcpu_t* cpu) -> bool
	{
		ia32_vmx_basic_register vmx_basic;
		vmx_basic.flags = __readmsr(IA32_VMX_BASIC);

		cpu->vmxon.revision_id = vmx_basic.vmcs_revision_id;
		cpu->vmxon.must_be_zero = 0;

		auto vmxon_phys = MmGetPhysicalAddress(&cpu->vmxon).QuadPart;
		NT_ASSERT(vmxon_phys % 0x1000 == 0);

		if (!vmx_on(vmxon_phys))
		{
			log_error("vmxon failed!");
			return false;
		}

		log_info("vmxon region phys created at -> %p", vmxon_phys);

		vmx::invept(invept_all_context, {});

		return true;
	}

	void enable_mtrr_exiting(vcpu_t* cpu)
	{
		ia32_mtrr_capabilities_register mtrr_cap;
		mtrr_cap.flags = __readmsr(IA32_MTRR_CAPABILITIES);

		enable_exit_for_msr_write(cpu->msr_bitmap, IA32_MTRR_DEF_TYPE, true);

		if (mtrr_cap.fixed_range_supported)
		{
			enable_exit_for_msr_write(cpu->msr_bitmap, IA32_MTRR_FIX64K_00000, true);
			enable_exit_for_msr_write(cpu->msr_bitmap, IA32_MTRR_FIX16K_80000, true);
			enable_exit_for_msr_write(cpu->msr_bitmap, IA32_MTRR_FIX16K_A0000, true);

			for (uint32_t i = 0; i < 8; ++i)
				enable_exit_for_msr_write(cpu->msr_bitmap, IA32_MTRR_FIX4K_C0000 + i, true);
		}

		for (uint32_t i = 0; i < mtrr_cap.variable_range_count; ++i) {
			enable_exit_for_msr_write(cpu->msr_bitmap, IA32_MTRR_PHYSBASE0 + i * 2, true);
			enable_exit_for_msr_write(cpu->msr_bitmap, IA32_MTRR_PHYSMASK0 + i * 2, true);
		}
	}

	auto setup_external_structures(vcpu_t* cpu) -> void
	{
		memset(&cpu->msr_bitmap, 0, sizeof(cpu->msr_bitmap));
		enable_exit_for_msr_read(cpu->msr_bitmap, IA32_FEATURE_CONTROL, true);

		enable_mtrr_exiting(cpu);
		memset(&cpu->host_tss, 0, sizeof(cpu->host_tss));

		prepare_host_idt(cpu->host_idt);
		prepare_host_gdt(cpu->host_gdt, &cpu->host_tss);
	}
}

