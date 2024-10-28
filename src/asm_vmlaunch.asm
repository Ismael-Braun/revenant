.code

?vm_launch@hv@@YA_NXZ proc
  mov rax, 681Ch
  vmwrite rax, rsp

  mov rax, 681Eh
  mov rdx, successful_launch
  vmwrite rax, rdx

  vmlaunch

  xor al, al
  ret

successful_launch:
  mov al, 1
  ret
?vm_launch@hv@@YA_NXZ endp

end

