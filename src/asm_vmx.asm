.code

?invept@vmx@@YAXW4invept_type@@AEBUinvept_descriptor@@@Z proc
  invept rcx, oword ptr [rdx]
  ret
?invept@vmx@@YAXW4invept_type@@AEBUinvept_descriptor@@@Z endp

?invvpid@vmx@@YAXW4invvpid_type@@AEBUinvvpid_descriptor@@@Z proc
  invvpid rcx, oword ptr [rdx]
  ret
?invvpid@vmx@@YAXW4invvpid_type@@AEBUinvvpid_descriptor@@@Z endp

?memcpy_safe@vmx@@YAXAEAUhost_exception_info@1@PEAXPEBX_K@Z proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  ; store RSI and RDI
  push rsi
  push rdi

  mov rsi, r8
  mov rdi, rdx
  mov rcx, r9

  rep movsb

ehandler:
  ; restore RDI and RSI
  pop rdi
  pop rsi

  ret
?memcpy_safe@vmx@@YAXAEAUhost_exception_info@1@PEAXPEBX_K@Z endp

?xsetbv_safe@vmx@@YAXAEAUhost_exception_info@1@I_K@Z proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  ; idx
  mov ecx, edx

  ; value (low part)
  mov eax, r8d

  ; value (high part)
  mov rdx, r8
  shr rdx, 32

  xsetbv

ehandler:
  ret
?xsetbv_safe@vmx@@YAXAEAUhost_exception_info@1@I_K@Z endp

?wrmsr_safe@vmx@@YAXAEAUhost_exception_info@1@I_K@Z proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  ; msr
  mov ecx, edx

  ; value
  mov eax, r8d
  mov rdx, r8
  shr rdx, 32

  wrmsr

ehandler:
  ret
?wrmsr_safe@vmx@@YAXAEAUhost_exception_info@1@I_K@Z endp

?rdmsr_safe@vmx@@YA_KAEAUhost_exception_info@1@I@Z proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  ; msr
  mov ecx, edx

  rdmsr

  ; return value
  shl rdx, 32
  and rax, rdx

ehandler:
  ret
?rdmsr_safe@vmx@@YA_KAEAUhost_exception_info@1@I@Z endp

?vmx_vmcall@hypercalls@@YA_KAEAUinput@1@@Z proc

  mov rax, [rcx]       ; code
  mov rdx, [rcx + 10h] ; args[1]
  mov r8,  [rcx + 18h] ; args[2]
  mov r9,  [rcx + 20h] ; args[3]
  mov r10, [rcx + 28h] ; args[4]
  mov r11, [rcx + 30h] ; args[5]
  mov rcx, [rcx + 08h] ; args[0]

  vmcall

  ret
?vmx_vmcall@hypercalls@@YA_KAEAUinput@1@@Z endp

end
