# Range Analysis Pass

A simple intra-procedural integer range analysis implemented as an LLVM Function pass (new PassManager plugin). The pass performs a Kildall-style dataflow analysis over basic blocks to compute conservative lower/upper bounds for integer values and memory locations. It is intended as a teaching / diagnostic implementation rather than a production-quality analyzer.

---

## Goals

* Provide a compact, readable implementation of a forward numeric range analysis (interval analysis).
* Compute conservative integer ranges for values defined within a function (constants, binary operations, PHI nodes, loads/stores, and some conditional-branch refinement).
* Print a summary of computed ranges for program variables at function exits.

---

## Key characteristics

* **Analysis type:** Forward, intra-procedural, monotone dataflow (Kildall worklist algorithm).
* **Domain:** Integer intervals (closed ranges) with two special lattice elements:

  * `FULL_RANGE = [INT_MIN, INT_MAX]` (top / unknown)
  * `EMPTY_RANGE = [INT_MAX, INT_MIN]` (bottom / infeasible)
* **Tracked values:** LLVM `Value*` keys in maps representing computed ranges. The implementation maps pointers (addresses) to ranges when storing/loading and treats them as memory locations.
* **Operations supported:** `Add`, `Sub`, `Mul` (with clamped overflow semantics to `INT_MIN`/`INT_MAX`).
* **Branch refinement:** When a predecessor terminator is a conditional `branch` whose condition is an `ICmpInst` comparing a loaded variable against an integer constant, the analysis intersect the predecessor's out-range with a refined constraint based on the branch predicate (e.g., `x < K` narrows to `[-inf, K-1]` on the true edge).
* **PHI handling:** PHI nodes are assigned the merge (union) of incoming ranges using `getRangeFromPred` for each incoming value.
* **Calls:** Pointer-typed call arguments are conservatively set to `FULL_RANGE` (unknown) in the caller's state.
* **Backedges handling:** If a backedge changes a variable range (which indicates a loop), the analysis conservatively widens the predecessor's out-range to `FULL_RANGE` for that variable. This prevents unsound under-approximation across loops.

---

## Files

* `RangeAnalysisPass.cpp` — the pass implementation (the code you provided).

---

## Design & Implementation Notes

### Lattice & Representation

* Ranges are represented by `Range { int lower, int upper; }`.
* `FULL_RANGE` uses `INT_MIN..INT_MAX` and `EMPTY_RANGE` uses `INT_MAX..INT_MIN`.
* Arithmetic operations (`addRange`, `subRange`, `mulRange`) use 64-bit temporaries and a `reality_check` clamp to prevent UB and to remain within `INT_MIN`/`INT_MAX`.

### Dataflow

* The pass instantiates `in` and `out` maps from `BasicBlock*` to `BasicBlockState`.
* Worklist seeded with all basic blocks (initially optimistic). Entry block arguments are seeded with `FULL_RANGE`.
* The meet operator merges incoming `BasicBlockState`s by calling `BasicBlockState::meet` which unions ranges (via `mergeRange`) and returns whether any range changed.
* For each instruction, we update the `nmistate` (the block's new-out state) according to the instruction semantics described above.

### Control-flow and branch refinement

* When a predecessor `br` is conditional, and its condition is an `ICmpInst` comparing a `LoadInst` (variable load) against a `ConstantInt`, we attempt to refine the range of the loaded pointer for that successor using the comparison predicate and whether the successor is the true or false branch.
* The predicate is swapped / inverted as needed to account for which successor the current block is.

### Backedges and loops

* We use `FindFunctionBackedges` to find function-level back edges. If the predecessor is a loop backedge to the current block and the incoming predecessor's out-state range for a variable differs from the current block's in-state range, we conservatively widen the predecessor's range to `FULL_RANGE` for that variable. This prevents the algorithm from incorrectly converging to a too-narrow range across loops.

### Result presentation

* The pass constructs a map `programVariables` by scanning debug info (`DbgVariableRecord`) and function arguments (named ones) so that the final printed output uses readable variable names where available.
* The final per-variable ranges are computed by meeting (`mergeRange`) the `out` states of function exit blocks (basic blocks with no successors) to obtain `finalState` and then printing the range of each program variable.

---

## Build (example)

Use a minimal CMake setup for building a plugin against your LLVM installation. Example `CMakeLists.txt` snippet:

```cmake
cmake_minimum_required(VERSION 3.13)
project(RangeAnalysisPass)
find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}/cmake")
include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})
add_library(RangeAnalysisPass MODULE RangeAnalysisPass.cpp)
set_target_properties(RangeAnalysisPass PROPERTIES
  PREFIX ""
  COMPILE_FLAGS "${LLVM_COMPILE_FLAGS}"
)
llvm_update_compile_flags(RangeAnalysisPass)
target_link_libraries(RangeAnalysisPass PRIVATE
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

The plugin will be produced as `RangeAnalysisPass.so` (or `.dylib` on macOS).

---

## Usage

Load the plugin into `opt` and run the pass over a bitcode file (`.bc`) or IR (`.ll`):

```bash
opt -load-pass-plugin=./RangeAnalysisPass.so -passes="range-analysis" -disable-output example.bc
```

The pass prints results to `stderr` (LLVM's `errs()`).

---

## Example output (illustrative)

```
Function foo
x : [0, 10]
y : [5, 5]
ptr : [INT_MIN, INT_MAX]

```

If debug info is available, you may see named local variables. Otherwise, LLVM values (e.g. `%1`, `%2`) appear via their `Value::getName()` string where present.

---

## Limitations & Caveats

* **Conservative, not precise:** This is deliberately conservative: most unknowns default to `FULL_RANGE`.
* **Pointer handling is naive:** The pass models memory using pointer `Value*` as keys and does not perform alias analysis. Different pointers that may alias are not disambiguated.
* **Overflow handling:** Arithmetic clamps results to `INT_MIN`/`INT_MAX` rather than modeling modular overflow semantics.
* **Limited predicates support:** Only `ICmp` patterns comparing a `LoadInst` to a `ConstantInt` are used to refine ranges across branches. More complex comparisons are ignored.
* **Performance:** The pass uses a per-basic-block map and iterates over all instructions and pairs of preds; for large functions this can be slower than a tuned implementation.
* **Reliance on debug info:** The final pretty-printed variable names depend on debug info found via `filterDbgVars`. If absent, variable names may be unavailable.

---

## Possible extensions

* Integrate LLVM Alias Analysis (AA) to reason about overlapping pointers and model memory more precisely.
* Add support for more operations (div, shifts, bitwise ops) and improved handling of signed/unsigned semantics.
* Implement widening for loops more carefully (e.g., widen gradually or use loop iteration counts from ScalarEvolution when available).
* Make the analysis interprocedural (or summarize callees) for better precision.
* Output results as JSON to drive external tools.
* Add command-line options for verbosity, focus on specific functions, or to limit tracked values.

---

## Testing

Create small C/C++ testcases, compile them to LLVM IR (`clang -O0 -emit-llvm -S test.c -o test.ll`) and run the pass with `opt` to inspect the printed ranges. Add regression tests with `lit`/`FileCheck` if you plan to integrate into an LLVM test suite.
