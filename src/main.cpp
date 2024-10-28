#include "hv.h"
#include <ntddk.h>

auto driver_entry(PDRIVER_OBJECT driver, PUNICODE_STRING) -> NTSTATUS
{

   if (!hv::start_hv())
   {
       log_error("failed to virtualize system");
       return STATUS_HV_OPERATION_FAILED;
   }

    return STATUS_SUCCESS;
}