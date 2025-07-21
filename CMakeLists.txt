cmake_minimum_required(VERSION 3.15)
project(mms-server VERSION 1.0.0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
set(TEST_OUTPUT_DIR  ${PROJECT_SOURCE_DIR}/bin/tests)


if(CMAKE_BUILD_TYPE STREQUAL Debug)  #STREQUAL是字符串比较的运算符  debug 要调试信息 -g -pipi使用管道加快编译速度
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -pipe -Wall -Wno-builtin-macro-redefined -Wno-unused-variable -Wno-deprecated-declarations -lrt -pthread")
elseif (NOT CMAKE_BUILD_TYPE STREQUAL Debug) # 不是debug，那就是Release，-o3启动所有的编译器优化
    message(STATUS "Release Version")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -g -pipe -Wall -Wno-builtin-macro-redefined -Wno-unused-variable -Wno-deprecated-declarations -lrt -pthread")
endif()

# -fno-strict-aliasing：
# 关闭严格的别名优化。别名优化是一种编译器优化技术，它假定不同类型的指针不会指向相同的内存位置。在某些情况下，这种优化可能导致不正确的代码行为。使用这个选项可以避免这种问题，特别是在编写低级别代码或与旧代码库兼容时很有用。
# -Wno-error=shadow：
# 禁用将变量遮蔽警告视为错误。变量遮蔽（shadowing）指的是在内层作用域声明了一个与外层作用域中同名的变量，这可能导致混淆和潜在的错误。默认情况下，编译器可能会发出警告并将其视为错误，但这个选项会让编译器忽略这个警告。
# -Wno-sign-compare：
# 禁用符号比较警告。当有符号类型（例如 int）与无符号类型（例如 unsigned int）进行比较时，编译器会发出警告，因为这可能导致未定义行为或意外的结果。这个选项会让编译器忽略这种警告。
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing -Wno-error=shadow -Wno-sign-compare")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(./cmake/3rd_lib.cmake)
include(./cmake/utils.cmake)

include_directories(tmms)

add_subdirectory(tmms)

add_subdirectory(tests)
