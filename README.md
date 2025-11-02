# LLVM Program Analysis Suite

## Overview

<<<<<<< HEAD
<<<<<<< HEAD
This repository provides a modular collection of static analysis passes implemented on top of the **LLVM compiler infrastructure**.
Each analysis is built as a **loadable LLVM plugin**, enabling independent development, testing, and integration into the LLVM optimization pipeline.

The project includes a **top-level automated CMake build system** that:

* Detects your installed LLVM
* Builds all analysis plugins
* Automatically generates sample C programs
* Compiles those test programs into LLVM IR (`.ll`) for immediate use with `opt`
=======
This repository is a collection of **static analysis passes** implemented on top of the **LLVM compiler infrastructure**.
Each pass performs a different kind of program analysis on LLVM IR — such as range estimation, pointer aliasing, and dependence detection — and can be independently compiled and run as an LLVM plugin.

The suite is modular and designed for extensibility: new analyses can be easily added as separate subdirectories following a consistent structure.
>>>>>>> bf355c4 (Added Pointer Analysis, Dependence Analysis, and  Range Analysis)
=======
This repository provides a modular collection of static analysis passes implemented on top of the **LLVM compiler infrastructure**.
Each analysis is built as a **loadable LLVM plugin**, enabling independent development, testing, and integration into the LLVM optimization pipeline.

The project includes a **top-level automated CMake build system** that:

* Detects your installed LLVM
* Builds all analysis plugins
* Automatically generates sample C programs
* Compiles those test programs into LLVM IR (`.ll`) for immediate use with `opt`
>>>>>>> 29707e2 (Adding cmake)

---

## Project Structure

```
<<<<<<< HEAD
<<<<<<< HEAD
LLVM_Analysis/
├── ArrayInstrumentation/     # Runtime checks for array accesses
│   ├── arrayinstrumentation.cc
│   └── README.md
│
├── Dependence_analysis/      # Loop and memory dependence analysis
│   ├── Dependence_analysis.cc
│   └── README.md
│
├── Pointer_analysis/         # Andersen-style pointer alias analysis
│   ├── Pointer_Analysis.cc
│   └── README.md
│
├── Range_analysis/           # Abstract-interpretation-based integer range analysis
│   ├── Range_analysis.cc
│   └── README.md
│
├── CMakeLists.txt            
└── README.md
```

Each submodule contains:

* The LLVM pass source
* Module-specific CMake
* Usage documentation and examples

---

## Included Analyses

### Range Analysis

Determines numerical bounds of integer variables using abstract interpretation.
Enables overflow detection, bounds inference, and loop reasoning.

### Pointer Analysis

Implements Andersen’s inclusion-based points-to analysis to identify potential aliasing.
Enables conservative optimization and memory-safety analysis.

### Dependence Analysis

Performs loop-carried and memory dependence detection used in parallelization and scheduling.

### Array Instrumentation

Injects instrumentation into array accesses for dynamic bounds validation and error detection.

---

## Build Requirements

* LLVM with CMake configuration support (`llvm-config` available)
* CMake ≥ 3.13
* C++17 compatible compiler
* `clang` in PATH for test generation

---

## Building All Analyses

```bash
git clone https://github.com/anubhavkhajuria/LLVM_Analysis.git
cd LLVM_Analysis
mkdir build && cd build
cmake .. -DLLVM_DIR=$(llvm-config --cmakedir)
make -j$(nproc)
```

This automatically:

* Compiles all passes → `build/lib/*.so`
* Generates sample test programs → `build/tests/*.c`
* Produces compiled IR files → `build/tests/*.ll`

---

## Running an Analysis

Navigate to the test directory:

```bash
cd build/tests
```

Run any plugin with `opt`.
Example: ArrayInstrumentation

```bash
opt -load-pass-plugin=../lib/libArrayInstrumentation.so \
    -passes="array-instrumentation" \
    -disable-output \
    test_array.ll
```

Each submodule README provides the exact pass name for execution.

---


## References

* LLVM: Writing an LLVM Pass
  [https://llvm.org/docs/WritingAnLLVMPass.html](https://llvm.org/docs/WritingAnLLVMPass.html)

* Andersen, L. O.
  *Program Analysis and Specialization for the C Programming Language*, 1994

* Muchnick, S. S.
  *Advanced Compiler Design and Implementation*
=======
LLVM-Analysis/
├── Dependence_analysis/   # Loop & memory dependence detection
│   ├── DependenceAnalysis.cpp
=======
LLVM_Analysis/
├── ArrayInstrumentation/     # Runtime checks for array accesses
│   ├── arrayinstrumentation.cc
>>>>>>> 29707e2 (Adding cmake)
│   └── README.md
│
├── Dependence_analysis/      # Loop and memory dependence analysis
│   ├── Dependence_analysis.cc
│   └── README.md
│
├── Pointer_analysis/         # Andersen-style pointer alias analysis
│   ├── Pointer_Analysis.cc
│   └── README.md
│
├── Range_analysis/           # Abstract-interpretation-based integer range analysis
│   ├── Range_analysis.cc
│   └── README.md
│
├── CMakeLists.txt            
└── README.md
```

Each submodule contains:

* The LLVM pass source
* Module-specific CMake
* Usage documentation and examples

---

## Included Analyses

### Range Analysis

Determines numerical bounds of integer variables using abstract interpretation.
Enables overflow detection, bounds inference, and loop reasoning.

### Pointer Analysis

Implements Andersen’s inclusion-based points-to analysis to identify potential aliasing.
Enables conservative optimization and memory-safety analysis.

### Dependence Analysis

Performs loop-carried and memory dependence detection used in parallelization and scheduling.

### Array Instrumentation

Injects instrumentation into array accesses for dynamic bounds validation and error detection.

---

## Build Requirements

* LLVM with CMake configuration support (`llvm-config` available)
* CMake ≥ 3.13
* C++17 compatible compiler
* `clang` in PATH for test generation

---

## Building All Analyses

```bash
git clone https://github.com/anubhavkhajuria/LLVM_Analysis.git
cd LLVM_Analysis
mkdir build && cd build
cmake .. -DLLVM_DIR=$(llvm-config --cmakedir)
make -j$(nproc)
```

This automatically:

* Compiles all passes → `build/lib/*.so`
* Generates sample test programs → `build/tests/*.c`
* Produces compiled IR files → `build/tests/*.ll`

---

## Running an Analysis

Navigate to the test directory:

```bash
cd build/tests
```

Run any plugin with `opt`.
Example: ArrayInstrumentation

```bash
opt -load-pass-plugin=../lib/libArrayInstrumentation.so \
    -passes="array-instrumentation" \
    -disable-output \
    test_array.ll
```

Each submodule README provides the exact pass name for execution.

---


## References

* LLVM: Writing an LLVM Pass
  [https://llvm.org/docs/WritingAnLLVMPass.html](https://llvm.org/docs/WritingAnLLVMPass.html)

<<<<<<< HEAD
>>>>>>> bf355c4 (Added Pointer Analysis, Dependence Analysis, and  Range Analysis)
=======
* Andersen, L. O.
  *Program Analysis and Specialization for the C Programming Language*, 1994

* Muchnick, S. S.
  *Advanced Compiler Design and Implementation*
>>>>>>> 29707e2 (Adding cmake)
