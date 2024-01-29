#include "vmexit.h"
#include "vcpu.h"
#include "handlers.h"
#include "trap-frame.h"
#include "timing.h"

using namespace vmx;

namespace hv
{
	static void dispatch_vm_exit(vcpu_t* const cpu, vmx_vmexit_reason const reason)
	{
		switch (reason.basic_exit_reason)
		{
			case VMX_EXIT_REASON_EXCEPTION_OR_NMI:             handlers::exception_or_nmi(cpu);    break;
			case VMX_EXIT_REASON_EXECUTE_GETSEC:               handlers::getsec(cpu);              break;
			case VMX_EXIT_REASON_EXECUTE_INVD:                 handlers::invd(cpu);                break;
			case VMX_EXIT_REASON_NMI_WINDOW:                   handlers::nmi_window(cpu);          break;
			case VMX_EXIT_REASON_EXECUTE_CPUID:                handlers::cpuid(cpu);               break;
			case VMX_EXIT_REASON_MOV_CR:                       handlers::mov_cr(cpu);              break;
			case VMX_EXIT_REASON_EXECUTE_RDMSR:                handlers::rdmsr(cpu);               break;
			case VMX_EXIT_REASON_EXECUTE_WRMSR:                handlers::wrmsr(cpu);               break;
			case VMX_EXIT_REASON_EXECUTE_XSETBV:               handlers::xsetbv(cpu);              break;
			case VMX_EXIT_REASON_EXECUTE_VMXON:                handlers::vmxon(cpu);               break;
			case VMX_EXIT_REASON_EXECUTE_VMCALL:               handlers::vmcall(cpu);              break;
			case VMX_EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED: handlers::vmx_preemption(cpu);      break;
			case VMX_EXIT_REASON_EXECUTE_RDTSC:                handlers::rdtsc(cpu);               break;
			case VMX_EXIT_REASON_EXECUTE_RDTSCP:               handlers::rdtscp(cpu);              break;
			case VMX_EXIT_REASON_EPT_VIOLATION:                handlers::ept_violation(cpu);       break;
			case VMX_EXIT_REASON_EPT_MISCONFIGURATION:         handlers::ept_misconfiguration(cpu);break;
			case VMX_EXIT_REASON_EXECUTE_INVEPT:
			case VMX_EXIT_REASON_EXECUTE_INVVPID:
			case VMX_EXIT_REASON_EXECUTE_VMCLEAR:
			case VMX_EXIT_REASON_EXECUTE_VMLAUNCH:
			case VMX_EXIT_REASON_EXECUTE_VMPTRLD:
			case VMX_EXIT_REASON_EXECUTE_VMPTRST:
			case VMX_EXIT_REASON_EXECUTE_VMREAD:
			case VMX_EXIT_REASON_EXECUTE_VMRESUME:
			case VMX_EXIT_REASON_EXECUTE_VMWRITE:
			case VMX_EXIT_REASON_EXECUTE_VMXOFF:
			case VMX_EXIT_REASON_EXECUTE_VMFUNC:               handlers::vmx_instruction(cpu);    break;
			default:
			{
				inject_hw_exception(general_protection, 0);
				break;
			}
		}
	}

	bool handle_vm_exit(guest_registers* const ctx)
	{
		auto const cpu = reinterpret_cast<vcpu_t*>(_readfsbase_u64());
		cpu->ctx = ctx;

		vmx_vmexit_reason reason;
		reason.flags = static_cast<uint32_t>(vm_read(VMCS_EXIT_REASON));

		cpu->hide_vm_exit_overhead = false;

		dispatch_vm_exit(cpu, reason);

		hide_vm_exit_overhead(cpu);

		vm_write(VMCS_CTRL_TSC_OFFSET, cpu->tsc_offset);
		vm_write(VMCS_GUEST_VMX_PREEMPTION_TIMER_VALUE, cpu->preemption_timer);

		cpu->ctx = nullptr;

		return false;
	}

	void handle_host_interrupt(trap_frame* const frame)
	{
		switch (frame->vector)
		{
		case nmi:
		{
			auto ctrl = read_ctrl_proc_based();
			ctrl.nmi_window_exiting = 1;
			write_ctrl_proc_based(ctrl);

			auto const cpu = reinterpret_cast<vcpu_t*>(_readfsbase_u64());
			++cpu->queued_nmis;

			break;
		}
		default:
		{
			if (!frame->r10 || !frame->r11)
			{
				segment_descriptor_register_64 idtr;
				idtr.base_address = frame->rsp;
				idtr.limit = 0xFFF;
				__lidt(&idtr);

				break;
			}

			frame->rip = frame->r10;

			auto const e = reinterpret_cast<host_exception_info*>(frame->r11);

			e->exception_occurred = true;
			e->vector = frame->vector;
			e->error = frame->error;

			frame->r10 = 0;
			frame->r11 = 0;
		}
		}
	}
}