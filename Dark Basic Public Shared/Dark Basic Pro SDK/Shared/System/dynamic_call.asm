.model flat, c
.safeseh asm_dynamic_call
.code

; extern "C" DWORD __cdecl asm_dynamic_call(void* func, const DWORD* args, int argc);
asm_dynamic_call proc
    push ebp
    mov ebp, esp
    push edi
    push esi
    push ebx

    mov ecx, [ebp + 16] ; argc
    mov edx, [ebp + 12] ; args
    
    test ecx, ecx
    jz do_call
    
    lea esi, [edx + ecx*4 - 4] ; Point to the last argument (push right-to-left)

push_loop:
    push dword ptr [esi]
    sub esi, 4
    dec ecx
    jnz push_loop

do_call:
    call dword ptr [ebp + 8] ; func

    ; Restore registers using the EBP frame pointer, because ESP could be anywhere
    ; depending on the callee's calling convention (__stdcall vs __cdecl)
    mov ebx, dword ptr [ebp - 12]
    mov esi, dword ptr [ebp - 8]
    mov edi, dword ptr [ebp - 4]
    mov esp, ebp
    pop ebp
    ret
asm_dynamic_call endp
end
