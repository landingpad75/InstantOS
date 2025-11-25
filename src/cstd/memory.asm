; credits to https://github.com/xyve7/fast_mem/blob/master/mem.asm

; rax memset(rdi, rsi, rdx);
; void* memset(void* dest, int val, size_t count);
global memset
memset:
    ; move rsi into rax (the second parameter into rax so stosb can copy it)
    mov rax, rsi
    ; move rdx into rcx (the count so that rep can use it as a count)
    mov rcx, rdx
    ; save the rdi register which has the destination address, which we will mov into rax later
    mov r8, rdi
    ; repeat the fill
    rep stosb
    ; restore the original address of rdi and return it
    mov rax, r8
    ret

global memset32
memset32:
    mov eax, esi        ; 32-bit fill value in EAX
    mov rcx, rdx        ; count
    rep stosd           ; store EAX into [RDI], repeatedly (32-bit)
    mov rax, rdi        ; return dest
    ret
; rax memcpy(rdi, rsi, rdx);
; void* memcpy(void* dest, void* src, size_t count);
global memcpy
memcpy:
    ; save the rdi register which has the destination address as the return value
    mov rax, rdi
    ; move rdx into rcx (the count so that rep can use it as a count)
    mov rcx, rdx
    ; repeat the fill
    rep movsb
    ret

; rax memmove(rdi, rsi, rdx);
; void* memmove(void* dest, const void* src, size_t count);
global memmove
memmove:
    ; save the rdi register which has the destination address as the return value
    mov rax, rdi
    ; move rdx into rcx (the count so that rep can use it as a count)
    mov rcx, rdx

    ; compare rsi and rdi
    cmp rsi, rdi
    jb .backwards_copy

    ; regular memcpy
    rep movsb
    jmp .done

.backwards_copy:
    ; set the direction flag so the string operations DECREMENT instead of INCREMENT
    std
    ; make the address of rdi and rsi to point to the END of the block
    lea rdi, [rdi + rdx - 1]
    lea rsi, [rsi + rdx - 1]
    ; copy the data
    rep movsb
    ; clear the direction flag, setting it back to increment
    cld
.done:
    ret

; rax memcmp(rdi, rsi, rdx);
; int memcmp(const void* b1, const void* b2, size_t count);
global memcmp
memcmp:
    ; move rdx into rcx (the count so that repe can use it as a count)
    mov rcx, rdx
    repe cmpsb
    je .eq

    ; difference between the last compared byte
    mov al, BYTE [rdi - 1]
    sub al, BYTE [rsi - 1]

    ; load the difference into eax with a sign extend
    movsx eax, al
    jmp .done
.eq:
    ; eax because the return value is an int
    xor eax, eax
.done:
    ret
; rax memchr(rdi, rsi, rdx);
; void* memchr(const void* b1, int val, size_t count);
global memchr
memchr:
    ; move rdx into rcx (the count so that repne can use it as a count)
    mov rcx, rdx
    ; move rdx into rcx (the count so that rep can use it as a count)
    mov rax, rsi
    repne scasb
    je .found

    ; not found, return null pointer
    xor rax, rax
    jmp .done
.found:
    ; decrement the point so it points to the byte which was found
    dec rdi
    mov rax, rdi
.done:
    ret

; rax memrchr(rdi, rsi, rdx);
; void* memrchr(const void* b1, int val, size_t count);
global memrchr
memrchr:
    ; move rdx into rcx (the count so that repne can use it as a count)
    mov rcx, rdx
    ; move rdx into rcx (the count so that rep can use it as a count)
    mov rax, rsi

    ; point to the end of the block
    lea rdi, [rdi + rdx - 1]

    ; change the direction flag to decrement
    std
    ; scan the string
    repne scasb
    je .found

    ; not found, return null pointer
    xor rax, rax
    jmp .done
.found:
    ; decrement the point so it points to the byte which was found
    inc rdi
    mov rax, rdi
.done:
    ; reset the decrement flag and return
    cld
    ret