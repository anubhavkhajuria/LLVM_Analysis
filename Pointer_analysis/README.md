# Pointer Analysis Pass (Andersen-style)

This plugin implements a simple, flow-insensitive, field-insensitive Andersen-style points-to analysis for LLVM `Function`s. It is intended as an educational implementation and diagnostic tool to help compiler engineers understand pointer relationships produced by a conservative, inclusion-based algorithm.

> The implementation is contained in `PointerAnalysis.cpp` (the code you provided). The pass runs at module scope and constructs a `PointerAnalysis` instance per function to compute a points-to map for local allocas, globals, and pointer-producing instructions.

---

## Goals

* Produce a conservative points-to relation using an inclusion-based (Andersen) algorithm.
* Track memory objects (`alloca`s and `GlobalVariable`s) as abstract memory locations.
* Handle common pointer-producing instructions (`bitcast`, `getelementptr`, `inttoptr`, `phi`, `select`, `call` producing pointers) as copy edges; handle `load`/`store` as load/store edges.
* Present a simple API and human-readable output listing intersections of points-to sets for selected program variables.

---

## Algorithm overview

The analysis implemented follows these main steps:

1. **Collect memory objects (`memObj`)**: Aggregate all global variables and `alloca` instructions in the function as abstract memory objects.

2. **Initialize points-to map**: Create an entry in `pointsTo` for every memory object and every instruction/value that may hold a pointer.

3. **Seed initial points-to edges**

   * For global variable initializers that reference a pointer to another memory object, add the initializer edge (e.g., `@g = <pointer to ...>` means `g -> target`).
   * For direct stores where both pointer operand and value operand are concrete memory objects, immediately insert the corresponding points-to relation into the mem object's points-to set.
   * For `inttoptr`, `call` returning pointer, `bitcast`, `getelementptr`, `phi`, `select` and pointer-typed instructions, add *copy edges* (inclusion constraints) to the `copy` vector.
   * For loads and stores where the pointer operand is not a known memory object, record *load* and *store* edges (to be processed by the analysis fixpoint).
   * For `call` instructions with pointer arguments, the code adds `store` edges between pointer arguments to reflect potential aliasing via callees (a conservative shortcut).

4. **Solve constraints to fixpoint**

   * Repeatedly process `copy`, `store`, and `load` constraints until no points-to set changes.
   * `pCopy` propagates targets along `x = y` edges.
   * `pStore` models `*p = q` by propagating `q`'s targets into every memory object `p` may point to (if the memory object holds pointers).
   * `pLoad` models `x = *p` by propagating the points-to set of each memory object `p` may refer to into `x`.

5. **Report results**

   * The pass collects known program variable names from global variables and debug info (`DbgVariableRecord`) and uses that map to translate memory object `Value*` back to readable names when printing intersection sets.
   * The current example prints the intersection of the points-to sets for two variables named `a` and `b`. If both are found in the `pointsTo` map, their intersection's element names are printed as `{"x" "y" ...}`.

---

## Files

* `PointerAnalysis.cpp` — the pointer analysis implementation (the code you provided).

---

## Build (example)

Use a minimal CMake setup for building a plugin against your LLVM installation. Example `CMakeLists.txt` snippet:

```cmake
cmake_minimum_required(VERSION 3.13)
project(PointerAnalysisPass)
find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}/cmake")
include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})
add_library(PointerAnalysisPass MODULE PointerAnalysis.cpp)
set_target_properties(PointerAnalysisPass PROPERTIES
  PREFIX ""
  COMPILE_FLAGS "${LLVM_COMPILE_FLAGS}"
)
llvm_update_compile_flags(PointerAnalysisPass)
target_link_libraries(PointerAnalysisPass PRIVATE
  LLVMCore
  LLVMSupport
)
```

Build:

```bash
mkdir build && cd build
cmake -G "Unix Makefiles" -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm ..
make -j
```

The plugin will be produced as `PointerAnalysisPass.so` (or `.dylib` on macOS).

---

## Usage

Load the plugin into `opt` and run the pass over a bitcode file (`.bc`) or IR (`.ll`):

```bash
opt -load-pass-plugin=./PointerAnalysisPass.so -passes="pointer-analysis" -disable-output example.bc
```

The pass prints results to `stderr` (LLVM's `errs()`).

---

## Example output (illustrative)

If the code finds named variables `a` and `b` it prints the names of memory objects that appear in both points-to sets; for example:

```
{ foo bar }
```

An empty intersection is printed as:

```
{ }
```

If the named variables cannot be found in debug info or as globals then the pass may print `Not sure what's happening here` (the current code path for debugging).

---

## Design notes & data structures

* **`memObj`** — `SmallPtrSet<const Value*,32>` holding abstract memory objects (globals and allocas).
* **`pointsTo`** — `DenseMap<const Value*, SmallPtrSet<const Value *,8>>` mapping a pointer value to the set of memory objects it may point to.
* **`copy`, `load`, `store`** — vectors of constraints captured while scanning the function. They represent inclusion/copy constraints, `x = *p` loads, and `*p = x` stores respectively.
* **`getPtrObj` / `getPtrOpd`** — utilities to normalize pointer values and to extract underlying memory objects from constant expressions (e.g., bitcast of a global initializer).

---

## Limitations & caveats

* **Flow-insensitive:** The analysis ignores control flow ordering — it is safe but less precise for temporally dependent pointer updates.
* **Field-insensitive:** The analysis treats aggregates as a single memory object; it does not track offsets within structs/arrays.
* **No context sensitivity or function summaries:** Call sites are modeled conservatively (the code adds copy/store edges for pointer arguments), but there is no interprocedural propagation across callees.
* **Conservative for `inttoptr` / `call` / `Global` initializers:** For some constructs the pass creates copy edges from every memory object to the pointer-producing value (e.g. `inttoptr` and pointer-returning calls), which may be overly conservative.
* **No type-based disambiguation beyond pointer vs non-pointer:** The `pStore` step checks whether the memory object's type is pointer-typed before copying stored pointer targets into it; however, aliasing between distinct memory objects is not resolved.
* **Not tuned for performance:** The implementation is readable and simple rather than optimized. The fixpoint iteration is straightforward and may be slower on large functions.

---

## Extensions & improvements

If you want to improve precision or performance consider:

* Integrating LLVM's Alias Analysis or Type-based alias analysis (TBAA) to better partition memory objects.
* Implementing field-sensitivity by splitting aggregate objects by offsets.
* Adding call-graph construction and interprocedural propagation to compute more precise summaries for callees.
* Using a worklist keyed by changed nodes to avoid scanning the entire set of constraints on each iteration.
* Adding diagnostics to print full points-to sets in a machine-readable format (JSON) for downstream tooling.

---

## Testing

Create small C testcases with pointers and compile them to LLVM IR:

```bash
clang -O0 -emit-llvm -S test.c -o test.ll
opt -load-pass-plugin=./PointerAnalysisPass.so -passes="pointer-analysis" -disable-output test.ll
```

Write lit/FileCheck tests if you plan to include the pass in an LLVM-style regression suite.


