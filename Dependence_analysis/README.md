# LoopDependenceAnalysisPass

A lightweight LLVM analysis pass (plugin) that performs **pairwise memory dependence analysis** for each loop in a function and prints a human-readable summary to `stderr`.

This repository contains a Module-level pass that iterates over all non-declaration functions, queries LLVM analyses (DependenceAnalysis, LoopInfo, ScalarEvolution) via the new PassManager, and for every loop collects memory-accessing instructions (loads, stores and atomics) and runs pairwise `DependenceAnalysis` checks. When a dependence is found it prints classification (flow/anti/output/input/unordered) and — when available — per-loop-level direction and distance (from `FullDependence`).

---

## Key features

* Walks all top-level and nested loops (using `LoopInfo`).
* Collects memory instructions in each loop (`LoadInst`, `StoreInst`, `AtomicCmpXchgInst`, `AtomicRMWInst`).
* Uses `DependenceAnalysis` to test pairwise dependences between memory accesses.
* Prints dependence classification and, when possible, per-level direction/distance (for `FullDependence`).
* Integrates with LLVM's **new PassManager** as a plugin (no legacy pass registration).

---

## Why this pass

Understanding memory dependences inside loops is crucial for correctness-preserving transformations (loop parallelization, vectorization, loop interchange). This pass is intended as a diagnostic tool for compiler developers and researchers who need a compact programmatic summary of loop-level memory dependences produced by LLVM's `DependenceAnalysis`.

---

## Files

* `LoopDependenceAnalysisPass.cpp` — the pass implementation (the code you provided).

---

## Build (example)

This pass is a plugin for LLVM. You can build it using `clang++/cmake` against your LLVM installation. Example (CMake-based) steps you can adapt to your environment:

1. Create a `CMakeLists.txt` referencing LLVM's `LLVMConfig.cmake` and target `LLVM::Core` / analyses. A minimal example used in many plugins:

```cmake
cmake_minimum_required(VERSION 3.13)
project(LoopDependenceAnalysisPass)
find_package(LLVM REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}/cmake")
include(AddLLVM)
add_definitions(${LLVM_DEFINITIONS})
add_library(LoopDependenceAnalysisPass MODULE LoopDependenceAnalysisPass.cpp)
set_target_properties(LoopDependenceAnalysisPass PROPERTIES
  PREFIX ""
  COMPILE_FLAGS "${LLVM_COMPILE_FLAGS}"
)
llvm_update_compile_flags(LoopDependenceAnalysisPass)
target_link_libraries(LoopDependenceAnalysisPass PRIVATE
  LLVMCore
  LLVMSupport
  LLVMAnalysis
)
```

2. Configure & build:

```bash
mkdir build && cd build
cmake -G "Unix Makefiles" -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm ..
make -j
```

The built plugin will typically be a shared object: e.g. `LoopDependenceAnalysisPass.so` (or on macOS `LoopDependenceAnalysisPass.dylib`).

---

## Usage

Once built, load the plugin into `opt` (the LLVM optimizer) and run it over a bitcode file (`.bc`) or IR (`.ll`) to see the pass output printed to `stderr`.

Example:

```bash
opt -load-pass-plugin=./LoopDependenceAnalysisPass.so -passes="dependence-analysis" -disable-output example.bc
```

Notes:

* `-disable-output` prevents `opt` from printing transformed IR — this pass only analyzes and prints diagnostics.
* If you prefer using the `-analyze` option style: the pass registers a custom pipeline name `dependence-analysis`, so the `-passes` invocation as shown above works for the new pass manager.

---

## Example output (excerpt)

Below is an illustrative (shortened) example of what the pass prints to `stderr` when analyzing a function containing loops. (The exact formatting and SCEV printing follow LLVM's `raw_ostream` formatting and `SCEV::print` output.)

```
dependence analysis for function: foo ===
Loop header: loop.header (depth=0) - memory accesses: 4
  Pair: Src=loop.bb:3 (i32*)  Dst=loop.bb:5 (i32*) -> DEPENDENCE: [Flow] [Consistent]
    level[0] direction=EQ distance=5
    level[1] direction=LT distance=<unknown>
  Pair: Src=loop.bb:3 (i32*)  Dst=loop.bb:7 (i32*) -> NO_DEPENDENCE
  Pair: Src=loop.bb:5 (i32*)  Dst=loop.bb:3 (i32*) -> DEPENDENCE: [Anti] [LoopIndependent]
    level[0] direction=GT
```

This shows the classification (Flow/Anti/Output...) and — for `FullDependence` results — per-loop level direction and the SCEV distance when available.

---

## Design notes

* The pass uses `FunctionAnalysisManager` via `ModuleAnalysisManager` (`FunctionAnalysisManagerModuleProxy`) to obtain per-function analyses. This is the recommended pattern for module-level passes that need function-level analysis results in the *new* pass manager.

* Memory instructions are collected conservatively (all loads/stores/atomics in loop blocks). This may include accesses that are not true candidates for loop-carried dependences (e.g., accesses to distinct stack slots) — the heavy lifting is done by `DependenceAnalysis`.

* `DependenceAnalysis::depends` returns a `std::unique_ptr<Dependence>`. If non-null, the `Dependence` object might be a `FullDependence` (which provides `getDirection`/`getDistance`) or a base `Dependence` with limited information. The pass attempts to `dyn_cast` to `FullDependence` to extract direction/distance. When casting to `FullDependence`, the pass takes ownership and deletes the object when done.

* The pass prints debug-friendly source locations using `Instruction::getDebugLoc()` when present, falling back to the basic block name and instruction index.

---

## Limitations & Caveats

* The analysis is only as accurate as LLVM's `DependenceAnalysis`. If `DependenceAnalysis` returns a confused result or lacks distance information, the pass will reflect that (it prints `[Confused]` or `minimal info / confused analysis`).

* The pass currently examines **every pair** of memory accesses inside a loop; this is quadratic in the number of memory instructions and may be slow for loops with many memory accesses. Consider filtering (e.g., by address space, pointer provenance) if you need a faster diagnostic.


