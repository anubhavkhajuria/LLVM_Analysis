# LLVM Program Analysis Suite

## Overview

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

---

## Project Structure

```
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
│   └── README.md
│
├── Pointer_analysis/      # Andersen-style pointer alias analysis
│   ├── PointerAnalysis.cpp
│   └── README.md
│
├── Range_analysis/        # Abstract interpretation–based range analysis
│   ├── RangeAnalysis.cpp
│   └── README.md
│
└── CMakeLists.txt         # (Optional) root CMake to build all submodules
```

Each subdirectory contains:

* The source code for the LLVM analysis pass.
* A `README.md` explaining the logic, usage, and sample tests.

---

## Key Analyses

###Range Analysis

Estimates numerical bounds for program variables using **abstract interpretation**.
Useful for detecting overflows, redundant checks, and loop bound optimization.

###Pointer Analysis

Implements **Andersen’s inclusion-based pointer alias analysis** to determine possible points-to relations between pointer variables.
Essential for alias detection, memory safety analysis, and optimization.

###Dependence Analysis

Performs **loop-carried and memory dependence analysis** to identify data dependencies between instructions or iterations.
Forms the basis for **automatic parallelization** and **loop transformation** passes.

---

## Building the Analyses

You can build all analyses together using CMake:

```bash
mkdir build && cd build
cmake -DLT_LLVM_INSTALL_DIR=$(llvm-config --prefix) ..
make
```

Or build individual analyses separately by entering their directories:

```bash
cd Pointer_analysis
mkdir build && cd build
cmake -DLT_LLVM_INSTALL_DIR=$(llvm-config --prefix) ..
make
```

---

## Running a Pass

1. Generate LLVM IR:

   ```bash
   clang -S -emit-llvm test.c -o test.ll
   ```

2. Run a pass using `opt`:

   ```bash
   opt -load-pass-plugin ./libPointerAnalysisPass.so -passes="pointer-analysis" test.ll
   ```

Each submodule’s README contains detailed examples for its respective pass.

---

## Adding a New Analysis

To add a new analysis:

1. Create a new directory under the project root, e.g.:

   ```
   MyNewAnalysis/
   ```
2. Implement your analysis pass as a `.cpp` file.
3. Add a `README.md` describing it.
4. Update the top-level `CMakeLists.txt` to include the new subdirectory:

   ```cmake
   add_subdirectory(MyNewAnalysis)
   ```

---

## Root `CMakeLists.txt`

Below is a sample top-level `CMakeLists.txt` you can use to build all analyses at once:

```cmake
cmake_minimum_required(VERSION 3.13)
project(LLVMAnalysisSuite LANGUAGES CXX)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Add each analysis subdirectory
add_subdirectory(Dependence_analysis)
add_subdirectory(Pointer_analysis)
add_subdirectory(Range_analysis)

# Add future analyses here
# add_subdirectory(MyNewAnalysis)
```

---

## Future Roadmap

* [ ] Add **interprocedural analysis** capabilities.
* [ ] Combine multiple analyses for **automatic parallelization**.
* [ ] Add test suite for regression and validation.

---

## References

* [LLVM Pass Infrastructure](https://llvm.org/docs/WritingAnLLVMPass.html)
* Andersen, L. O. *Program Analysis and Specialization for the C Programming Language*, DIKU Report, 1994.
* Muchnick, S. S. *Advanced Compiler Design and Implementation.*

>>>>>>> bf355c4 (Added Pointer Analysis, Dependence Analysis, and  Range Analysis)
