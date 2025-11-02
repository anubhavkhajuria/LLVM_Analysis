# LLVM Program Analysis Suite

## Overview

This repository provides a modular collection of static analysis passes implemented on top of the **LLVM compiler infrastructure**.  
Each analysis is built as a **loadable LLVM plugin**, enabling independent development, testing, and integration into the LLVM optimization pipeline.

The project includes a **top-level automated CMake build system** that:
- Detects your installed LLVM
- Builds all analysis plugins
- Automatically generates sample C programs
- Compiles those test programs into LLVM IR (`.ll`) for immediate use with `opt`

---

## Project Structure

LLVM_Analysis/
├── ArrayInstrumentation/ # Runtime checks for array accesses
│ ├── <source files>
│ └── README.md
│
├── Dependence_analysis/ # Loop and memory dependence analysis
│ ├── <source files>
│ └── README.md
│
├── Pointer_analysis/ # Andersen-style pointer alias analysis
│ ├── <source files>
│ └── README.md
│
├── Range_analysis/ # Abstract-interpretation-based integer range analysis
│ ├── <source files>
│ └── README.md
│
├── CMakeLists.txt 
└── README.md
yaml
Copy code

Each submodule contains:
- The LLVM pass source
- Module-specific CMake
- Usage documentation and examples

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

- LLVM with CMake configuration support (`llvm-config` available)
- CMake ≥ 3.13
- C++17 compatible compiler
- `clang` in PATH for test generation

---

## Building All Analyses

```bash
git clone https://github.com/anubhavkhajuria/LLVM_Analysis.git
cd LLVM_Analysis
mkdir build && cd build
cmake .. -DLLVM_DIR=$(llvm-config --cmakedir)
make -j$(nproc)
This automatically:

Compiles all passes → build/lib/*.so

Generates sample test programs → build/tests/*.c

Produces compiled IR files → build/tests/*.ll

Running an Analysis
Navigate to the test directory:

bash
Copy code
cd build/tests
Run any plugin with opt.
Example: ArrayInstrumentation

bash
Copy code
opt -load-pass-plugin=../lib/libArrayInstrumentation.so \
    -passes="array-instrumentation" \
    -disable-output \
    test_array.ll
Each submodule README provides the exact pass name for execution.

Adding a New Analysis
Create a new directory in the project root, e.g.:

Copy code
MyNewAnalysis/
Add your .cpp implementation and a local CMakeLists.txt

Write a small README.md explaining usage

Register it inside the root CMakeLists.txt:

cmake
Copy code
add_subdirectory(MyNewAnalysis)
After this, it will be built automatically with the rest of the suite.

References
LLVM: Writing an LLVM Pass
https://llvm.org/docs/WritingAnLLVMPass.html

Andersen, L. O.
Program Analysis and Specialization for the C Programming Language, 1994

Muchnick, S. S.
Advanced Compiler Design and Implementation

yaml
Copy code
