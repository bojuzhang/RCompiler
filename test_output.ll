=== Starting Complete Semantic Analysis ===
Step 1: Symbol Collection
Step 2: Constant Evaluation
Step 3: Control Flow Analysis
Step 4: Type Checking
=== Complete Semantic Analysis Completed ===
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
