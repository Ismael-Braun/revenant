#pragma once

#include "guest_registers.h"
#include "gdt.h"
#include "idt.h"
#include "vmx.h"

struct vcpu_cached_data
{
    u64 max_phys_addr;

    u64 vmx_cr0_fixed0;
    u64 vmx_cr0_fixed1;
    u64 vmx_cr4_fixed0;
    u64 vmx_cr4_fixed1;

    u64 xcr0_unsupported_mask;

    ia32_feature_control_register feature_control;
    ia32_feature_control_register guest_feature_control;

    ia32_vmx_misc_register vmx_misc;

    cpuid_eax_01 cpuid_01;
};

typedef struct vcpu_t
{
    alignas(0x1000) vmxon vmxon;
    alignas(0x1000) vmcs vmcs;
    alignas(0x1000) vmx_msr_bitmap msr_bitmap;
    alignas(0x1000) u8 host_stack[0x6000];

    alignas(0x1000) segment_descriptor_interrupt_gate_64 host_idt[hv::host_idt_descriptor_count];
    alignas(0x1000) segment_descriptor_32 host_gdt[hv::host_gdt_descriptor_count];
    alignas(0x1000) task_state_segment_64 host_tss;

    struct alignas(0x10)
    {
        vmx_msr_entry tsc;
        vmx_msr_entry perf_global_ctrl;
        vmx_msr_entry aperf;
        vmx_msr_entry mperf;
    } msr_exit_store;

    struct alignas(0x10)
    {
        vmx_msr_entry aperf;
        vmx_msr_entry mperf;
    } msr_entry_load;

    vcpu_cached_data cached;
    guest_registers* ctx;

    uint32_t volatile queued_nmis;

    u64 tsc_offset;
    u64 preemption_timer;
    u64 vm_exit_tsc_overhead;
    u64 vm_exit_mperf_overhead;
    u64 vm_exit_ref_tsc_overhead;

    bool hide_vm_exit_overhead;
};

namespace hv
{
    auto virtualize_cpu(vcpu_t* cpu) -> bool;
    auto cache_cpu_data(vcpu_cached_data& cached) -> void;

    auto setup_vmx(vcpu_t* cpu) -> bool;
    auto setup_vmcs(vcpu_t* cpu) -> bool;
    auto setup_vmxon(vcpu_t* cpu) -> bool;
    auto setup_external_structures(vcpu_t* cpu) -> void;
}

