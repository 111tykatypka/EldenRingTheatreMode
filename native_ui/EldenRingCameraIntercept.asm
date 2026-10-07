; Independently written x64 bridge for the verified 53-byte native camera-copy block.
; Preserve incoming flags, volatile GPRs and XMM0..5 across the C++ callback.
; Native copying is suppressed only after a validated override has been written.
EXTERN tm_camera_intercept:PROC
EXTERN tm_camera_original:QWORD
EXTERN tm_camera_continue:QWORD
PUBLIC tm_camera_detour

restore_context MACRO
    movups xmm0, [rsp+20h]
    movups xmm1, [rsp+30h]
    movups xmm2, [rsp+40h]
    movups xmm3, [rsp+50h]
    movups xmm4, [rsp+60h]
    movups xmm5, [rsp+70h]
    mov rsp, rbx
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
ENDM

.code
tm_camera_detour PROC
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    push rbx
    mov rbx, rsp
    and rsp, -16
    sub rsp, 80h ; Win64 shadow space plus six volatile XMM registers
    movups [rsp+20h], xmm0
    movups [rsp+30h], xmm1
    movups [rsp+40h], xmm2
    movups [rsp+50h], xmm3
    movups [rsp+60h], xmm4
    movups [rsp+70h], xmm5
    mov rcx, [rbx+28h] ; captured destination (original RDX)
    mov rdx, [rbx+30h] ; native source (original RCX)
    call tm_camera_intercept
    test eax, eax
    jz native_copy
    restore_context
    ; Preserve the native non-owned parameters and the block's register results.
    mov eax, [rcx+54h]
    mov [rdx+54h], eax
    mov eax, [rcx+58h]
    mov [rdx+58h], eax
    mov eax, [rcx+5Ch]
    mov [rdx+5Ch], eax
    movaps xmm0, [rcx+30h]
    movaps xmm1, [rcx+40h]
    jmp QWORD PTR [tm_camera_continue]
native_copy:
    restore_context
    jmp QWORD PTR [tm_camera_original]
tm_camera_detour ENDP
END
