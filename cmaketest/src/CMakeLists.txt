#set(Main_SRCS main.cpp)

#add_executable(myApp ${Main_SRCS})

# Specify here the libraries this program depends on
#target_link_libraries(cmtst libcfgdump)

#install(TARGETS cmtst DESTINATION bin)

cmake_minimum_required(VERSION 3.0)
project(cmtst)

find_package(LLVM REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIR})
include_directories(${COMMON_INCLUDES})
link_directories(${LLVM_LIB_DIR})
add_definitions(${LLVM_DEFINITIONS})

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

#find_package(CLANG REQUIRED)
#find_package(Clang REQUIRED clangTooling libClang)
#include_directories(${CLANG_INCLUDE_DIRS})
#add_definitions(${Clang_DEFINITIONS})

#add_definitions("-std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LLVM_COMPILE_FLAGS} -Wall -fno-rtti -g -std=c++11 -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS")

# Now build our tools
file(GLOB_RECURSE k_srcfiles "*.cpp")
message(STATUS "File list ${k_srcfiles}")
add_executable(cmtst ${k_srcfiles})

install(TARGETS cmtst DESTINATION "${CMAKE_BINARY_DIR}/bin")

set(CLANG_LIBS
  -lclangAST
  -lclangAnalysis
  -lclangBasic
  -lclangDriver
  -lclangEdit
  -lclangFrontend
  -lclangFrontendTool
  -lclangLex
  -lclangParse
  -lclangSema
  -lclangEdit
  -lclangASTMatchers
	-lclangRewrite
	-lclangRewriteFrontend
	-lclangStaticAnalyzerFrontend
	-lclangStaticAnalyzerCheckers
	-lclangStaticAnalyzerCore
	-lclangSerialization
	-lclangToolingCore
	-lclangTooling
)

target_link_libraries(cmtst -Wl,--start-group ${CLANG_LIBS} -Wl,--end-group)

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs
	option
	mcparser
	bitreader
	core
	support)

# Link against LLVM libraries
target_link_libraries(cmtst ${llvm_libs})

#target_link_libraries(cmtst
#  LLVMX86AsmParser # MC, MCParser, Support, X86Desc, X86Info
#  LLVMX86Desc # MC, Support, X86AsmPrinter, X86Info
#  LLVMX86AsmPrinter # MC, Support, X86Utils
#  LLVMX86Info # MC, Support, Target
#  LLVMX86Utils # Core, Support
#  LLVMipo
#  LLVMScalarOpts
#  LLVMInstCombine
#  LLVMTransformUtils
#  LLVMipa
#  LLVMAnalysis
#  LLVMTarget
#  LLVMOption # Support
#  LLVMMCParser # MC, Support
#  LLVMMC # Object, Support
#  LLVMObject # BitReader, Core, Support
#  LLVMBitReader # Core, Support
#  LLVMCore # Support
#  LLVMSupport
#)
