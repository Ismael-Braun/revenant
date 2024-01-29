#include "hv.h"
#include "vcpu.h"
#include "vmx.h"

using namespace vmx;

namespace hv 
{
    hypervisor_t ghv;

    static bool create_hv()
    {
        memset(&ghv, 0, sizeof(ghv));

        ghv.vcpu_count = KeQueryActiveProcessorCount(nullptr);

        auto const arr_size = sizeof(vcpu_t) * ghv.vcpu_count;

        ghv.vcpus = reinterpret_cast<vcpu_t*>(ExAllocatePoolZero(NonPagedPool, arr_size, HV_POOL_TAG));

        ghv.ept = reinterpret_cast<ept_t*>(ExAllocatePoolZero(NonPagedPool, sizeof(ept_t), HV_POOL_TAG));

        ghv.ept->start();

        get_system_cr3(&ghv.system_cr3.flags);

        setup_page_tables();

        log_info("allocated %u VCPUs (0x%zX bytes)", ghv.vcpu_count, arr_size);
        log_info("system cr3 -> %p", ghv.system_cr3.flags);

        return true;
    }

    auto start_hv() -> bool
    {
        if (!create_hv())
            return false;

        NT_ASSERT(KeGetCurrentIrql() <= APC_LEVEL);

        for (unsigned long i = 0; i < ghv.vcpu_count; ++i)
        {
            auto const orig_affinity = KeSetSystemAffinityThreadEx(1ull << i);

            if (!virtualize_cpu(&ghv.vcpus[i]))
            {
                KeRevertToUserAffinityThreadEx(orig_affinity);
                return false;
            }

            KeRevertToUserAffinityThreadEx(orig_affinity);
        }

        return true;
    }

}

