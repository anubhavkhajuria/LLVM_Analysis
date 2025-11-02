# LLVM Program Analysis Suite

## Overview

This repository provides a modular collection of static analysis passes implemented on top of the **LLVM compiler infrastructure**. Each analysis is built as a **loadable LLVM plugin**, enabling independent development, testing, and integration into the LLVM optimization pipeline.

The project includes a **top-level automated CMake build system** that:

* Detects your installed LLVM
* Builds all analysis plugins
* Automatically generates sample C programs
* Compiles those test programs into LLVM IR (`.ll`) for immediate use with `opt`

---

## Project Structure

```
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
├── CMakeLists.txt            # Top-level build configuration
└── README.md                 # This file
```

Each submodule contains:

* The LLVM pass source code
* Module-specific CMake configuration
* Usage documentation and examples

---

## Included Analyses

### Range Analysis

Determines numerical bounds of integer variables using abstract interpretation. Enables overflow detection, bounds inference, and loop reasoning.

**Use cases:** Integer overflow detection, loop bound analysis, array bounds checking

### Pointer Analysis

Implements Andersen's inclusion-based points-to analysis to identify potential aliasing. Enables conservative optimization and memory-safety analysis.

**Use cases:** Alias analysis, memory safety verification, optimization enabling

### Dependence Analysis

Performs loop-carried and memory dependence detection used in parallelization and scheduling.

**Use cases:** Loop parallelization, vectorization, scheduling optimization

### Array Instrumentation

Injects runtime instrumentation into array accesses for dynamic bounds validation and error detection.

**Use cases:** Buffer overflow detection, memory safety debugging, runtime verification

---

## Build Requirements

* **LLVM** with CMake configuration support (`llvm-config` available)
* **CMake** ≥ 3.13
* **C++17** compatible compiler (GCC 7+, Clang 5+, or MSVC 2017+)
* **clang** in PATH for test generation

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

Run any plugin with `opt`. Example using **Array Instrumentation**:

```bash
opt -load-pass-plugin=../lib/libArrayInstrumentation.so \
    -passes="array-instrumentation" \
    -disable-output \
    test_array.ll
```

### Pass Names for Each Analysis

* **Array Instrumentation:** `array-instrumentation`
* **Dependence Analysis:** `dependence-analysis`
* **Pointer Analysis:** `pointer-analysis`
* **Range Analysis:** `range-analysis`

Refer to each submodule's README for detailed usage instructions and pass-specific options.

---

## Example Workflow

1. **Build the project:**
   ```bash
   mkdir build && cd build
   cmake .. -DLLVM_DIR=$(llvm-config --cmakedir)
   make -j$(nproc)
   ```

2. **Write your own test program** or use the generated ones:
   ```c
   // mytest.c
   int main() {
       int arr[10];
       for (int i = 0; i < 10; i++) {
           arr[i] = i * 2;
       }
       return 0;
   }
   ```

3. **Compile to LLVM IR:**
   ```bash
   clang -S -emit-llvm -O0 mytest.c -o mytest.ll
   ```

4. **Run analysis:**
   ```bash
   opt -load-pass-plugin=lib/libRangeAnalysis.so \
       -passes="range-analysis" \
       -disable-output \
       mytest.ll
   ```

---

## Adding New Analyses

To add a new analysis pass:

1. Create a new subdirectory: `NewAnalysis/`
2. Add your pass source: `NewAnalysis/NewAnalysis.cc`
3. Create a CMakeLists.txt following the pattern in existing modules
4. Update the top-level CMakeLists.txt to include your subdirectory
5. Document your pass in `NewAnalysis/README.md`

---

## Contributing

Contributions are welcome! Please:

* Follow the existing code structure and style
* Include tests for new analyses
* Update documentation accordingly
* Submit pull requests with clear descriptions

---

## References

* **LLVM: Writing an LLVM Pass**  
  [https://llvm.org/docs/WritingAnLLVMPass.html](https://llvm.org/docs/WritingAnLLVMPass.html)

* **Andersen, L. O.**  
  *Program Analysis and Specialization for the C Programming Language*, 1994

* **Muchnick, S. S.**  
  *Advanced Compiler Design and Implementation*, Morgan Kaufmann, 1997


---

## Contact

**Author:** Anubhav Khajuria  
**Repository:** [https://github.com/anubhavkhajuria/LLVM_Analysis](https://github.com/anubhavkhajuria/LLVM_Analysis)

For questions, issues, or suggestions, please open an issue on GitHub.