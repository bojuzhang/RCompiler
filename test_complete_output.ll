target triple = "riscv32-unknown-unknown-elf"

declare dso_local void @print(i8* nocapture readonly)
declare dso_local void @println(i8* nocapture readonly)
declare dso_local void @printInt(i32 signext)
declare dso_local void @printlnInt(i32 signext)
declare dso_local i8* @getString()
declare dso_local i32 @getInt()
declare dso_local i8* @malloc(i32)
declare dso_local i8* @builtin_memset(i8* nocapture writeonly, i8, i32)
declare dso_local i8* @builtin_memcpy(i8* nocapture writeonly, i8* nocapture readonly, i32)
declare dso_local void @exit(i32) noreturn
define void @add(i32* %return_slot, i32 %param_1, i32 %param_2) {
entry:
  ; Function prologue for add
  ; Setting up parameter scope
  ; Setting up return slot parameter for user-defined function
  %a_ptr = alloca i32, align 4
  store i32 %param_1, i32* %a_ptr
  %b_ptr = alloca i32, align 4
  store i32 %param_2, i32* %b_ptr
  ; Checking for tail expression
  ; Found tail expression, processing...
  %var_0 = load i32, i32* %a_ptr, align 4
  %var_1 = load i32, i32* %b_ptr, align 4
  %var_2 = add i32 %var_0, %var_1
  ; Tail expression generated value: %var_2
  ; User-defined function: storing to return slot
  ; Storing value %var_2 to return slot %return_slot
  ret void
  ; Function epilogue for add
}
define void @main() {
entry:
  ; Function prologue for main
  ; Setting up parameter scope
  %x_ptr = alloca i32, align 4
  %var_3 = add i32 0, 10
  store i32 %var_3, i32* %x_ptr, align 4
  %y_ptr = alloca i32, align 4
  %var_4 = add i32 0, 20
  store i32 %var_4, i32* %y_ptr, align 4
  %result_ptr = alloca i32, align 4
  %var_5 = load i32, i32* %x_ptr, align 4
  %var_6 = load i32, i32* %y_ptr, align 4
  %var_7 = alloca i32, align 4
  call void @add(i32* %var_7, i32 %var_5, i32 %var_6)
  %var_8 = load i32, i32* %var_7, align 4
  store i32 %var_8, i32* %result_ptr, align 4
  %var_9 = load i32, i32* %result_ptr, align 4
  call void @printlnInt(i32 %var_9)
  ; Checking for tail expression
  ; Found tail expression, processing...
  %var_10 = load i32, i32* %result_ptr, align 4
  call void @exit(i32 %var_10)
  ; Tail expression generated value: 
  ret void
  ; Function epilogue for main
}
