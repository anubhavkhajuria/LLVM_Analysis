# ArrayInstrumentationPass

A lightweight LLVM analysis and instrumentation pass (plugin) that performs **static range analysis** on array accesses and automatically inserts runtime bounds checks where static safety cannot be proven.

This repository contains a Function-level pass that uses abstract interpretation with interval analysis to track value ranges through the control flow graph. For each array access (via `GetElementPtrInst`), the pass determines whether the index is provably within bounds. If not, it instruments the code with dynamic bounds checks that return -1 on out-of-bounds access.

---

## Key features

* Performs **interval-based abstract interpretation** to compute value ranges at each program point.
* Tracks ranges for scalar variables, function arguments, and array elements.
* Implements **path-sensitive analysis** using branch condition refinement (conditional branches and switches).
* Detects **loop back-edges** and applies widening to ensure termination of the fixed-point computation.
* Automatically **instruments array accesses** with runtime bounds checks when static analysis cannot prove safety.
* Integrates with LLVM's **new PassManager** as a plugin.

---

## Why this pass

Array bounds violations are a major source of security vulnerabilities and undefined behavior. This pass demonstrates how static analysis can automatically identify and protect potentially unsafe array accesses, reducing the need for manual bounds checking while eliminating unnecessary checks for provably safe accesses.

---

## Analysis technique

The pass implements a **forward dataflow analysis** using abstract interpretation:

1. **Abstract domain**: Integer intervals `[low, high]` representing possible values
2. **Transfer functions**: Models arithmetic operations (`add`, `sub`, `mul`), loads, stores, and PHI nodes
3. **Join operator**: Union of intervals (widening at loop headers)
4. **Widening**: Expands growing ranges to infinity at back-edges to guarantee termination
5. **Path refinement**: Narrows ranges based on branch conditions (e.g., `if (x < 10)`)

The analysis maintains both:
- **Scalar value ranges**: Tracking integers and pointers
- **Array element ranges**: Per-index tracking with strong updates for constant indices

---

## Files

* `ArrayInstrumentationPass.cpp` — the pass implementation (analysis + instrumentation).

---

## Build (example)

This pass is a plugin for LLVM. You can build it using `clang++/cmake` against your LLVM installation. Example (CMake-based) steps you can adapt to your environment:

1. Create a `CMakeLists.txt` referencing LLVM's `LLVMConfig.cmake`:

```cmake
cmake_minimum_required(VERSION 3.13)
project(ArrayInstrumentationPass)
find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}/cmake")
include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})
add_library(ArrayInstrumentationPass MODULE ArrayInstrumentationPass.cpp)
set_target_properties(ArrayInstrumentationPass PROPERTIES
  PREFIX ""
  COMPILE_FLAGS "${LLVM_COMPILE_FLAGS}"
)
llvm_update_compile_flags(ArrayInstrumentationPass)
target_link_libraries(ArrayInstrumentationPass PRIVATE
  LLVMCore
  LLVMSupport
  LLVMAnalysis
  LLVMTransformUtils
)
```

2. Configure & build:

```bash
mkdir build && cd build
cmake -G "Unix Makefiles" -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm ..
make -j
```

The built plugin will typically be a shared object: e.g. `ArrayInstrumentationPass.so` (or on macOS `ArrayInstrumentationPass.dylib`).

---

## Usage

Once built, load the plugin into `opt` (the LLVM optimizer) and run it over a bitcode file (`.bc`) or IR (`.ll`). The pass will analyze and instrument array accesses in functions named `test`:

```bash
opt -load-pass-plugin=./ArrayInstrumentationPass.so -passes="instrument-array-accesses" -S example.bc -o example_instr.bc
```

Notes:

* The pass currently only processes functions named `test` (see `if (F.getName() != "test")` check).
* Use `-S` to output human-readable LLVM IR to see the inserted bounds checks.
* The pass prints diagnostic messages to `stderr` showing which accesses were instrumented and why.

---

## Example output (excerpt)

Below is an illustrative example of what the pass prints to `stderr` when analyzing array accesses:

```
Checking index: %3 = load i32, i32* %i
  Index range: [0,10]
  Final range: [0,10]
Should be safe [0,10] within [0,9]
Instrumenting   %5 = getelementptr inbounds [10 x i32], [10 x i32]* %arr, i64 0, i64 %3

Checking index: %8 = load i32, i32* %j
  Index loaded from: %j
  Ptr range: [-2147483648,2147483647]
  Final range: [-2147483648,2147483647]
It needs check (range: [-2147483648,2147483647], size: 10)
Instrumenting   %9 = getelementptr inbounds [10 x i32], [10 x i32]* %arr, i64 0, i64 %8
```

The instrumented code will contain:

```llvm
%bounds_check = icmp sge i32 %idx, 0
%bounds_check2 = icmp slt i32 %idx, 10
%in_bounds = and i1 %bounds_check, %bounds_check2
br i1 %in_bounds, label %cont.bb, label %err.bb

err.bb:
  ret i32 -1

cont.bb:
  %gep = getelementptr inbounds [10 x i32], [10 x i32]* %arr, i64 0, i64 %idx
  ; ... continue normal execution
```

---

## Design notes

* **Range representation**: Uses a custom `range` class with `safelane` (low) and `offlane` (high) bounds. Special values `r_bot` (unreachable/empty) and `r_top` (all integers) represent the lattice extremes.

* **Array modeling**: Arrays allocated via `AllocaInst` are tracked with per-element ranges when indices are constant or can be resolved to single values. Dynamic indices trigger a weak update that loses precision.

* **Path sensitivity**: The pass refines ranges based on branch conditions. For example, after `if (x >= 0 && x < 10)`, the true branch knows `x ∈ [0,9]`.

* **Widening strategy**: When a back-edge is detected, if a range is growing (low decreasing or high increasing), it's immediately widened to infinity in that direction. This ensures the fixed-point iteration terminates.

* **Instrumentation**: Bounds checks are inserted by splitting the basic block at the GEP instruction, creating an error block that returns -1, and adding a conditional branch based on the bounds check.

* **Function calls**: Conservatively invalidate pointer-derived state (arrays become `r_top`, tracked ranges are cleared).

---

## Limitations & Caveats

* **Function scope**: Currently only analyzes functions named `test`. Remove or modify the name check to analyze all functions.

* **Precision**: The interval domain is simple and loses precision with:
  - Complex loop induction variables
  - Multiplication (due to non-monotonic widening)
  - Aliasing (no pointer analysis)
  - Non-linear arithmetic

* **Soundness**: The analysis is intended to be sound (safe bounds checks are not removed incorrectly), but:
  - Does not model all LLVM instructions (unhandled instructions default to `r_top`)
  - Assumes no undefined behavior in the input program
  - May be imprecise with type conversions, bitcasts, and complex pointer arithmetic

* **Performance**: The fixed-point iteration is quadratic in basic blocks in the worst case. Deeply nested loops or large CFGs may be slow to analyze.

* **Instrumentation overhead**: Every unproven-safe array access gets a bounds check. In performance-critical code, consider profile-guided optimization or more precise analysis.

* **Error handling**: Currently returns -1 from the function on bounds violations. Production use might want to call an error handler, trap, or use other recovery mechanisms.

---

## Extending the pass

To improve precision or handle more patterns:

1. **Add relational domains**: Track relationships like `i < j` or `i = j + k`
2. **Implement narrowing**: After widening, narrow ranges on subsequent iterations
3. **Pointer analysis**: Distinguish different array allocations and handle aliasing
4. **Loop unrolling**: Partially unroll small loops for better precision
5. **Configurable instrumentation**: Add command-line flags to control when/how checks are inserted

---

## Related work

This pass implements techniques from:
- Abstract interpretation (Cousot & Cousot)
- Interval analysis for bounds checking
- LLVM's ScalarEvolution (more precise but limited to affine expressions)

For production bounds checking, see LLVM's AddressSanitizer or BoundsChecking passes.
