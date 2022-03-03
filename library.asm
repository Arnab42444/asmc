
  ITOA_BUF_LEN equ 32

  INPUT_BUF_LEN equ 1024
  MAX_SYMBOL_NAME_LEN equ 128
  SYMBOL_TABLE_LEN equ 1024
  ;; SYMBOL_TABLE_SIZE = SYMBOL_TABLE_LEN * MAX_SYMBOL_NAME_LEN
  SYMBOL_TABLE_SIZE equ 131072

section .bss

itoa_buf:
  resb ITOA_BUF_LEN

symbol_names_ptr:
  resd 1

symbol_locs_ptr:
  resd 1

symbol_num:
  resd 1

section .text

  ;; char *itoa(int x)
  global itoa
itoa:
  ;; Clear the buffer
  mov ecx, ITOA_BUF_LEN
  mov eax, itoa_buf

itoa_clear_loop:
  cmp ecx, 0
  je itoa_cleared
  mov BYTE [eax], SPACE
  add eax, 1
  sub ecx, 1
  jmp itoa_clear_loop

itoa_cleared:
  ;; Prepare registers
  mov eax, [esp+4]
  push esi
  mov esi, 10
  mov ecx, itoa_buf
  add ecx, ITOA_BUF_LEN
  sub ecx, 1

  ;; Store a terminator at the end
  mov BYTE [ecx], 0
  sub ecx, 1

itoa_loop:
  ;; Divide by the base
  mov edx, 0
  div esi

  ;; Output the remainder
  add edx, ZERO
  mov [ecx], dl

  ;; Check for termination
  cmp eax, 0
  je itoa_end

  ;; Move the pointer backwards
  sub ecx, 1

  jmp itoa_loop

itoa_end:
  mov eax, ecx
  pop esi
  ret


  ;; int atoi(char*)
  global atoi
atoi:
  ;; Call decode_number, adapting the parameters
  mov ecx, [esp+4]
  push 0
  mov edx, esp
  push edx
  push ecx
  call decode_number
  add esp, 8
  cmp eax, 0
  pop eax
  je platform_panic
  ret


  ;; void *memcpy(void *dest, const void *src, int n)
  global memcpy
memcpy:
  ;; Load registers
  mov eax, [esp+4]
  mov ecx, [esp+8]
  mov edx, [esp+12]
  push ebx

memcpy_loop:
  ;; Test for termination
  cmp edx, 0
  je memcpy_end

  ;; Copy one character
  mov bl, [ecx]
  mov [eax], bl

  ;; Decrease the counter and increase the pointers
  sub edx, 1
  add eax, 1
  add ecx, 1

  jmp memcpy_loop

memcpy_end:
  pop ebx
  ret


  global init_symbols
init_symbols:
  ;; Allocate symbol names table
  push SYMBOL_TABLE_SIZE
  call platform_allocate
  add esp, 4
  mov ecx, symbol_names_ptr
  mov [ecx], eax

  ;; Allocate symbol locations table
  mov eax, SYMBOL_TABLE_LEN
  mov edx, 4
  mul eax
  push eax
  call platform_allocate
  add esp, 4
  mov ecx, symbol_locs_ptr
  mov [ecx], eax

  ;; Reset symbol_num
  mov eax, symbol_num
  mov DWORD [eax], 0

  ret


  global find_symbol
find_symbol:
  ;; Set up registers and stack
  push ebp
  mov ebp, esp
  mov ecx, 0

find_symbol_loop:
  ;; Check for termination
  mov eax, symbol_num
  cmp ecx, [eax]
  je find_symbol_not_found

  ;; Save ecx
  push ecx

  ;; Compute and push the second argument to strcmp
  mov edx, MAX_SYMBOL_NAME_LEN
  mov eax, ecx
  mul edx
  mov ecx, symbol_names_ptr
  add eax, [ecx]
  push eax

  ;; Push the first argument
  mov eax, [ebp+8]
  push eax

  ;; Call strcmp, clean the stack and restore ecx
  call strcmp
  add esp, 8
  pop ecx

  ;; If strcmp returned 0, then we return
  cmp eax, 0
  je find_symbol_found

  ;; Increment ecx and check for termination
  add ecx, 1
  jmp find_symbol_loop

find_symbol_found:
  mov eax, ecx
  jmp find_symbol_ret

find_symbol_not_found:
  mov eax, 0xffffffff
  jmp find_symbol_ret

find_symbol_ret:
  pop ebp
  ret


  global add_symbol
add_symbol:
  push ebp
  mov ebp, esp
  push ebx

  ;; Call strlen
  mov eax, [ebp+8]
  push eax
  call strlen
  add esp, 4

  ;; Check input length
  cmp eax, 0
  jna platform_panic
  cmp eax, MAX_SYMBOL_NAME_LEN
  jnb platform_panic

  ;; Branch to appropriate stage
  mov edx, stage
  mov eax, [edx]
  cmp eax, 0
  je add_symbol_stage0
  cmp eax, 1
  je add_symbol_stage1
  jmp platform_panic

add_symbol_stage0:
  ;; Call find_symbol
  mov eax, [ebp+8]
  push eax
  call find_symbol
  add esp, 4

  ;; Check that the symbol does not exist yet
  cmp eax, 0xffffffff
  jne platform_panic

  ;; Put the current symbol number in ebx and check it is not
  ;; overflowing
  mov eax, symbol_num
  mov ebx, [eax]
  cmp ebx, SYMBOL_TABLE_LEN
  jnb platform_panic

  ;; Save the location for the new symbol
  mov eax, ebx
  mov ecx, 4
  mul ecx
  mov ecx, symbol_locs_ptr
  add eax, [ecx]
  mov ecx, [ebp+12]
  mov [eax], ecx

  ;; Save the name for the new symbol
  mov eax, [ebp+8]
  push eax
  mov eax, ebx
  mov ecx, MAX_SYMBOL_NAME_LEN
  mul ecx
  mov ecx, symbol_names_ptr
  add eax, [ecx]
  push eax
  call strcpy
  add esp, 8

  ;; Increment and store the new symbol number
  add ebx, 1
  mov eax, symbol_num
  mov [eax], ebx

  jmp add_symbol_ret

add_symbol_stage1:
  ;; Call find_symbol
  mov eax, [ebp+8]
  push eax
  call find_symbol
  add esp, 4

  ;; Check it is smaller than the symbol number
  mov ecx, symbol_num
  mov edx, [ecx]
  cmp eax, edx
  jnb platform_panic

  ;; Check the location matches with the symbol table
  mov ecx, 4
  mul ecx
  mov ecx, symbol_locs_ptr
  add eax, [ecx]
  mov ecx, [ebp+12]
  cmp [eax], ecx
  jne platform_panic

  jmp add_symbol_ret

add_symbol_ret:
  pop ebx
  pop ebp
  ret


  global get_symbol_loc
get_symbol_loc:
  mov eax, symbol_locs_ptr
  mov eax, [eax]
  ret

  global get_symbol_num
get_symbol_num:
  mov eax, symbol_num
  ret

  global get_current_loc
get_current_loc:
  mov eax, current_loc
  ret
