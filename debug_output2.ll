target triple = "riscv32-unknown-unknown-elf"

declare dso_local void @print(ptr nocapture readonly)
declare dso_local void @println(ptr nocapture readonly)
declare dso_local void @printInt(i32 signext)
declare dso_local void @printlnInt(i32 signext)
declare dso_local ptr @getString()
declare dso_local i32 @getInt()
declare dso_local ptr @malloc(i32)
declare dso_local ptr @builtin_memset(ptr nocapture writeonly, i8, i32)
declare dso_local ptr @builtin_memcpy(ptr nocapture writeonly, ptr nocapture readonly, i32)
declare dso_local void @exit(i32) noreturn
; Generated function signature: void @main()
define void @main() {
entry:
  ; Function prologue for main
  ; Setting up parameter scope
  %a_ptr = alloca i32, align 4
  %var_0 = add i32 0, 10
  store i32 %var_0, i32* %a_ptr, align 4
  %b_ptr = alloca i32, align 4
  %var_1 = add i32 0, 3
  store i32 %var_1, i32* %b_ptr, align 4
  %c_ptr = alloca i32, align 4
  %var_2 = add i32 0, 2
  store i32 %var_2, i32* %c_ptr, align 4
  %result_ptr = alloca i32, align 4
  %var_3 = load i32, i32* %a_ptr, align 4
  %var_4 = load i32, i32* %b_ptr, align 4
  %var_5 = load i32, i32* %c_ptr, align 4
  %var_6 = sub i32 %var_4, %var_5
  %var_7 = sub i32 %var_3, %var_6
  store i32 %var_7, i32* %result_ptr, align 4
  %var_8 = load i32, i32* %result_ptr, align 4
  call void @printlnInt(i32 %var_8)
  %var_9 = add i32 0, 0
  call void @exit(i32 %var_9)
  ; Checking for tail expression
  ret void
  ; Function epilogue for main
}
