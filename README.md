# LLVM Program Analysis Suite

## Overview

This repository is a collection of **static analysis and instrumentation passes** implemented on top of the **LLVM compiler infrastructure**.

Each pass performs a different kind of program analysis or transformation on LLVM IR — including range estimation, pointer aliasing, dependence detection, and automatic bounds checking — and can be independently compiled and run as an LLVM plugin.

The suite is modular and designed for extensibility: new analyses can be easily added as separate subdirectories following a consistent structure.

---

## Project Structure

```
LLVM-Analysis/
├── ArrayInstrumentation/  # Array bounds checking with range analysis
│   ├── arrayinstrumentation.cpp
│   └── README.md
│
├── Dependence_analysis/   # Loop & memory dependence detection
│   ├── Dependence_Analysis.cpp
│   └── README.md
│
├── Pointer_analysis/      # Andersen-style pointer alias analysis
│   ├── Pointer_Analysis.cpp
│   └── README.md
│
├── Range_analysis/        # Abstract interpretation–based range analysis
│   ├── Range_analysis.cpp
└── └── README.md
```

Each subdirectory contains:
* The source code for the LLVM analysis pass.
* A `README.md` explaining the logic, usage, and sample tests.

---

## Key Analyses

### Array Instrumentation
Combines **interval-based abstract interpretation** with **automatic instrumentation** to insert runtime bounds checks for array accesses that cannot be statically proven safe.
Useful for detecting buffer overflows, improving memory safety, and debugging array-related errors.

### Dependence Analysis
Performs **loop-carried and memory dependence analysis** to identify data dependencies between instructions or iterations using LLVM's DependenceAnalysis framework.
Forms the basis for **automatic parallelization** and **loop transformation** passes.

### Pointer Analysis
Implements **Andersen's inclusion-based pointer alias analysis** to determine possible points-to relations between pointer variables.
Essential for alias detection, memory safety analysis, and optimization.

### Range Analysis
Estimates numerical bounds for program variables using **abstract interpretation**.
Useful for detecting overflows, redundant checks, and loop bound optimization.

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
cd ArrayInstrumentation
mkdir build && cd build
cmake -DLLVM_DIR=$(llvm-config --cmakedir) ..
make
```

---

## Running a Pass

1. Generate LLVM IR:
   ```bash
   clang -S -emit-llvm test.c -o test.ll
   ```

2. Run an analysis pass using `opt`:
   ```bash
   # For analysis-only passes (no IR modification)
   opt -load-pass-plugin ./Dependence_Analysis.so \
       -passes="dependence-analysis" \
       -disable-output test.ll
   ```

3. Run an instrumentation pass:
   ```bash
   # For transformation passes that modify IR
   opt -load-pass-plugin ./arrayinstrumentation.so \
       -passes="instrument-array-accesses" \
       -S test.ll -o test_instr.ll
   ```

Each submodule's README contains detailed examples for its respective pass.

---

## Adding a New Analysis

To add a new analysis:

1. Create a new directory under the project root, e.g.:
   ```
   MyNewAnalysis/
   ```

2. Implement your analysis pass as a `.cpp` file following the new PassManager API.

3. Add a `README.md` describing the analysis technique, usage, and examples.

4. Create a local `CMakeLists.txt` in the subdirectory.

5. Update the top-level `CMakeLists.txt` to include the new subdirectory:
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
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add each analysis subdirectory
add_subdirectory(ArrayInstrumentation)
add_subdirectory(Dependence_analysis)
add_subdirectory(Pointer_analysis)
add_subdirectory(Range_analysis)

# Add future analyses here
# add_subdirectory(MyNewAnalysis)
```

---

## Analysis Categories

This suite contains two types of passes:

### Pure Analysis Passes
These passes analyze the IR without modifying it:
- **Dependence Analysis**: Reports memory dependencies
- **Pointer Analysis**: Computes points-to sets
- **Range Analysis**: Estimates value ranges

Usage: Typically run with `-disable-output` to see diagnostic output on stderr.

### Transformation/Instrumentation Passes
These passes modify the IR based on analysis results:
- **Array Instrumentation**: Inserts bounds checks based on range analysis

Usage: Use `-S` to output modified IR, or omit to generate optimized bitcode.

---

## Future Roadmap

* [ ] Add **interprocedural analysis** capabilities (cross-function range/pointer analysis).
* [ ] Implement **taint analysis** for security vulnerability detection.
* [ ] Add **control flow integrity** (CFI) instrumentation.
* [ ] Combine multiple analyses for **automatic parallelization**.
* [ ] Add comprehensive test suite for regression and validation.
* [ ] Support **profile-guided optimization** to reduce instrumentation overhead.
* [ ] Implement **symbolic execution** for path-sensitive analysis.

---

## References

* [LLVM Pass Infrastructure](https://llvm.org/docs/WritingAnLLVMPass.html)
* [LLVM New Pass Manager](https://llvm.org/docs/NewPassManager.html)
* Cousot, P. & Cousot, R. *Abstract Interpretation: A Unified Lattice Model for Static Analysis of Programs*, POPL 1977.
* Andersen, L. O. *Program Analysis and Specialization for the C Programming Language*, DIKU Report, 1994.
* Cytron, R. et al. *Efficiently Computing Static Single Assignment Form*, TOPLAS 1991.
* Muchnick, S. S. *Advanced Compiler Design and Implementation*, Morgan Kaufmann, 1997.

---

## Contributing

Contributions are welcome! When adding a new analysis:

1. Follow the existing code structure and naming conventions
2. Use the new PassManager API (not the legacy pass infrastructure)
3. Include comprehensive documentation in the README
4. Add test cases demonstrating the analysis
5. Ensure the pass builds cleanly with recent LLVM versions (14+)

---

## Contact

For questions, issues, or contributions, please open an issue on the repository or contact:

**Anubhab (Anubhav Khajuria)**  
Email: anubhavkhajuria5@gmail.com
