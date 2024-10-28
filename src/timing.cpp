#include "timing.h"
#include "vcpu.h"
#include "vmx.h"
#include "types.h"
#include "hypercalls.h"

using namespace vmx;

namespace hv 
{
    auto hide_vm_exit_overhead(vcpu_t* const cpu) -> void
    {
        ia32_perf_global_ctrl_register perf_global_ctrl;
        perf_global_ctrl.flags = cpu->msr_exit_store.perf_global_ctrl.msr_data;

        cpu->msr_entry_load.aperf.msr_data = cpu->msr_exit_store.aperf.msr_data;
        cpu->msr_entry_load.mperf.msr_data = cpu->msr_exit_store.mperf.msr_data;
        vm_write(VMCS_GUEST_PERF_GLOBAL_CTRL, perf_global_ctrl.flags);

        cpu->msr_entry_load.aperf.msr_data -= cpu->vm_exit_mperf_overhead;
        cpu->msr_entry_load.mperf.msr_data -= cpu->vm_exit_mperf_overhead;

        if (perf_global_ctrl.en_fixed_ctrn & (1ull << 2))
        {
            auto const cpl = current_guest_cpl();

            ia32_fixed_ctr_ctrl_register fixed_ctr_ctrl;
            fixed_ctr_ctrl.flags = __readmsr(IA32_FIXED_CTR_CTRL);

            if ((cpl == 0 && fixed_ctr_ctrl.en2_os) || (cpl == 3 && fixed_ctr_ctrl.en2_usr))
                __writemsr(IA32_FIXED_CTR2, __readmsr(IA32_FIXED_CTR2) - cpu->vm_exit_ref_tsc_overhead);
        }

        if (!cpu->hide_vm_exit_overhead || cpu->vm_exit_tsc_overhead > 10000)
        {
            cpu->tsc_offset = 0;

            cpu->preemption_timer = ~0ull;

            return;
        }

        cpu->preemption_timer = max(2, 10000 >> cpu->cached.vmx_misc.preemption_timer_tsc_relationship);

        cpu->tsc_offset -= cpu->vm_exit_tsc_overhead;
    }

    auto measure_vm_exit_tsc_overhead() -> u64
    {
        _disable();

        hypercalls::input hv_input;
        hv_input.code = hypercalls::hypercall_ping;
        hv_input.key = hypercalls::hv_key;

        uint64_t lowest_vm_exit_overhead = ~0ull;
        uint64_t lowest_timing_overhead = ~0ull;

        for (int i = 0; i < 10; ++i)
        {
            _mm_lfence();
            auto start = __rdtsc();
            _mm_lfence();

            _mm_lfence();
            auto end = __rdtsc();
            _mm_lfence();

            auto const timing_overhead = (end - start);

            vmx_vmcall(hv_input);

            _mm_lfence();
            start = __rdtsc();
            _mm_lfence();

            vmx_vmcall(hv_input);

            _mm_lfence();
            end = __rdtsc();
            _mm_lfence();

            auto const vm_exit_overhead = (end - start);

            if (vm_exit_overhead < lowest_vm_exit_overhead)
                lowest_vm_exit_overhead = vm_exit_overhead;
            if (timing_overhead < lowest_timing_overhead)
                lowest_timing_overhead = timing_overhead;
        }

        _enable();
        return lowest_vm_exit_overhead - lowest_timing_overhead;
    }

    auto measure_vm_exit_ref_tsc_overhead() -> u64
    {
        _disable();

        hypercalls::input hv_input;
        hv_input.code = hypercalls::hypercall_ping;
        hv_input.key = hypercalls::hv_key;

        ia32_fixed_ctr_ctrl_register curr_fixed_ctr_ctrl;
        curr_fixed_ctr_ctrl.flags = __readmsr(IA32_FIXED_CTR_CTRL);

        ia32_perf_global_ctrl_register curr_perf_global_ctrl;
        curr_perf_global_ctrl.flags = __readmsr(IA32_PERF_GLOBAL_CTRL);

        auto new_fixed_ctr_ctrl = curr_fixed_ctr_ctrl;
        new_fixed_ctr_ctrl.en2_os = 1;
        new_fixed_ctr_ctrl.en2_usr = 0;
        new_fixed_ctr_ctrl.en2_pmi = 0;
        new_fixed_ctr_ctrl.any_thread2 = 0;
        __writemsr(IA32_FIXED_CTR_CTRL, new_fixed_ctr_ctrl.flags);

        auto new_perf_global_ctrl = curr_perf_global_ctrl;
        new_perf_global_ctrl.en_fixed_ctrn |= (1ull << 2);
        __writemsr(IA32_PERF_GLOBAL_CTRL, new_perf_global_ctrl.flags);

        uint64_t lowest_vm_exit_overhead = ~0ull;
        uint64_t lowest_timing_overhead = ~0ull;

        for (int i = 0; i < 10; ++i)
        {
            _mm_lfence();
            auto start = __readmsr(IA32_FIXED_CTR2);
            _mm_lfence();

            _mm_lfence();
            auto end = __readmsr(IA32_FIXED_CTR2);
            _mm_lfence();

            auto const timing_overhead = (end - start);

            vmx_vmcall(hv_input);

            _mm_lfence();
            start = __readmsr(IA32_FIXED_CTR2);
            _mm_lfence();

            vmx_vmcall(hv_input);

            _mm_lfence();
            end = __readmsr(IA32_FIXED_CTR2);
            _mm_lfence();

            auto const vm_exit_overhead = (end - start);

            if (vm_exit_overhead < lowest_vm_exit_overhead)
                lowest_vm_exit_overhead = vm_exit_overhead;
            if (timing_overhead < lowest_timing_overhead)
                lowest_timing_overhead = timing_overhead;
        }

        __writemsr(IA32_PERF_GLOBAL_CTRL, curr_perf_global_ctrl.flags);
        __writemsr(IA32_FIXED_CTR_CTRL, curr_fixed_ctr_ctrl.flags);

        _enable();
        return lowest_vm_exit_overhead - lowest_timing_overhead;
    }

    auto measure_vm_exit_mperf_overhead() -> u64
    {
        _disable();

        hypercalls::input hv_input;
        hv_input.code = hypercalls::hypercall_ping;
        hv_input.key = hypercalls::hv_key;

        uint64_t lowest_vm_exit_overhead = ~0ull;
        uint64_t lowest_timing_overhead = ~0ull;

        for (int i = 0; i < 10; ++i)
        {
            _mm_lfence();
            auto start = __readmsr(IA32_MPERF);
            _mm_lfence();

            _mm_lfence();
            auto end = __readmsr(IA32_MPERF);
            _mm_lfence();

            auto const timing_overhead = (end - start);

            vmx_vmcall(hv_input);

            _mm_lfence();
            start = __readmsr(IA32_MPERF);
            _mm_lfence();

            vmx_vmcall(hv_input);

            _mm_lfence();
            end = __readmsr(IA32_MPERF);
            _mm_lfence();

            auto const vm_exit_overhead = (end - start);

            if (vm_exit_overhead < lowest_vm_exit_overhead)
                lowest_vm_exit_overhead = vm_exit_overhead;
            if (timing_overhead < lowest_timing_overhead)
                lowest_timing_overhead = timing_overhead;
        }

        _enable();
        return lowest_vm_exit_overhead - lowest_timing_overhead;
    }

}

