# LLVM Program Analysis Suite

## Overview

This repository is a collection of **static analysis passes** implemented on top of the **LLVM compiler infrastructure**.
Each pass performs a different kind of program analysis on LLVM IR — such as range estimation, pointer aliasing, and dependence detection — and can be independently compiled and run as an LLVM plugin.

The suite is modular and designed for extensibility: new analyses can be easily added as separate subdirectories following a consistent structure.

---

## Project Structure

```
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

