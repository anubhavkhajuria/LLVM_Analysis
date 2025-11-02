cmake_minimum_required(VERSION 3.13)

project(LLVM_AnalysisSuite LANGUAGES CXX)
<<<<<<< HEAD


=======
>>>>>>> 29707e2 (Adding cmake)
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "LLVMConfig.cmake at: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(ArrayInstrumentation)
add_subdirectory(Dependence_analysis)
add_subdirectory(Pointer_analysis)
add_subdirectory(Range_analysis)

<<<<<<< HEAD

=======
>>>>>>> 29707e2 (Adding cmake)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

function(generate_test_file name content)
    set(C_FILE ${CMAKE_BINARY_DIR}/tests/${name}.c)
    set(LL_FILE ${CMAKE_BINARY_DIR}/tests/${name}.ll)

    file(WRITE ${C_FILE} "${content}")

    add_custom_command(
        OUTPUT ${LL_FILE}
        COMMAND clang -S -emit-llvm -O0 -Xclang -disable-O0-optnone ${C_FILE} -o ${LL_FILE}
        DEPENDS ${C_FILE}
        COMMENT "Generating LLVM IR for ${name}.c"
    )

    add_custom_target(${name}_IR ALL DEPENDS ${LL_FILE})
endfunction()

generate_test_file(test_array "#include <stdio.h>
int main() {
    int arr[10];
    int idx;
    for(int i = 0; i < 10; i++){
        arr[i] = i * 2;
    }
<<<<<<< HEAD
    printf(\"arr[5] = %d\n\", arr[5]);
    scanf(\"%d\", &idx);
    if(idx >= 0 && idx < 10)
        printf(\"arr[%d] = %d\n\", idx, arr[idx]);
=======
    printf(\"arr[5] = %d\\n\", arr[5]);
    scanf(\"%d\", &idx);
    if(idx >= 0 && idx < 10)
        printf(\"arr[%d] = %d\\n\", idx, arr[idx]);
>>>>>>> 29707e2 (Adding cmake)
    return 0;
}")

generate_test_file(test_dependence "#include <stdio.h>
int computeSum(int n){
    int sum = 0;
    for(int i = 1; i <= n; i++)
        sum += i;
    return sum;
}
int fibonacci(int n){
    int a=0,b=1,c;
    for(int i=2;i<=n;i++){
        c=a+b; a=b; b=c;
    }
    return b;
}
int main(){
<<<<<<< HEAD
    printf(\"%d %d\n\", computeSum(100), fibonacci(10));
=======
    printf(\"%d %d\\n\", computeSum(100), fibonacci(10));
>>>>>>> 29707e2 (Adding cmake)
    return 0;
}")

generate_test_file(test_pointer "#include <stdio.h>
#include <stdlib.h>
void swap(int *a,int *b){int t=*a;*a=*b;*b=t;}
int main(){
    int x=5,y=10;
    swap(&x,&y);
<<<<<<< HEAD
    printf(\"%d %d\n\", x,y);
=======
    printf(\"%d %d\\n\", x,y);
>>>>>>> 29707e2 (Adding cmake)
    return 0;
}")

generate_test_file(test_range "#include <stdio.h>
int main(){
    int n=10,sum=0;
    for(int i=0;i<100;i++) sum+=i;
<<<<<<< HEAD
    printf(\"%d\n\",sum);
    return 0;
}")

message(STATUS "LLVM Analysis Suite Ready!")
=======
    printf(\"%d\\n\",sum);
    return 0;
}")

message(STATUS "\nLLVM Analysis Suite Ready!")
>>>>>>> 29707e2 (Adding cmake)
message(STATUS "Plugins in: ${CMAKE_BINARY_DIR}/lib")
message(STATUS "Tests in:   ${CMAKE_BINARY_DIR}/tests")
message(STATUS "Run with opt from tests directory.")
