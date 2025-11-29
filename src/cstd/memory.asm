; credits to https://github.com/xyve7/fast_mem/blob/master/mem.asm

; rax memset(rdi, rsi, rdx);
; void* memset(void* dest, int val, size_t count);
global memset
memset:
    cld

    mov rax, rsi
    mov rcx, rdx
    mov r8, rdi

    rep stosb

    mov rax, r8
    ret


; rax memset32(rdi, rsi, rdx);
; void* memset32(void* dest, uint32_t val, size_t count);
global memset32
memset32:
    mov eax, esi
    mov rcx, rdx

    rep stosd
    
    mov rax, rdi
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
; size_t strlen(const char* str);
global strlen
strlen:
    xor rax, rax
    mov rcx, -1
    xor al, al
    repne scasb
    not rcx
    dec rcx
    mov rax, rcx
    ret

; char* strcpy(char* dest, const char* src);
global strcpy
strcpy:
    mov rax, rdi
.loop:
    mov cl, [rsi]
    mov [rdi], cl
    inc rsi
    inc rdi
    test cl, cl
    jnz .loop
    ret

; char* strncpy(char* dest, const char* src, size_t n);
global strncpy
strncpy:
    mov rax, rdi
    mov rcx, rdx
.loop:
    test rcx, rcx
    jz .done
    mov r8b, [rsi]
    mov [rdi], r8b
    inc rsi
    inc rdi
    dec rcx
    test r8b, r8b
    jnz .loop
.pad:
    test rcx, rcx
    jz .done
    mov byte [rdi], 0
    inc rdi
    dec rcx
    jmp .pad
.done:
    ret

; int strcmp(const char* s1, const char* s2);
global strcmp
strcmp:
.loop:
    mov al, [rdi]
    mov cl, [rsi]
    cmp al, cl
    jne .diff
    test al, al
    jz .equal
    inc rdi
    inc rsi
    jmp .loop
.diff:
    movzx eax, al
    movzx ecx, cl
    sub eax, ecx
    ret
.equal:
    xor eax, eax
    ret

; int strncmp(const char* s1, const char* s2, size_t n);
global strncmp
strncmp:
    mov rcx, rdx
.loop:
    test rcx, rcx
    jz .equal
    mov al, [rdi]
    mov r8b, [rsi]
    cmp al, r8b
    jne .diff
    test al, al
    jz .equal
    inc rdi
    inc rsi
    dec rcx
    jmp .loop
.diff:
    movzx eax, al
    movzx r8d, r8b
    sub eax, r8d
    ret
.equal:
    xor eax, eax
    ret
