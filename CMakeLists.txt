cmake_minimum_required(VERSION 3.10)

# 项目信息
project(webserver)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)

# 设置输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 查找线程库
find_package(Threads REQUIRED)

# 查找主目录下的所有 .cpp 文件，但排除 CMake 自己生成的文件夹
file(GLOB_RECURSE SRC
    "${PROJECT_SOURCE_DIR}/*.cpp"
    "${PROJECT_SOURCE_DIR}/*.h"
    "${PROJECT_SOURCE_DIR}/*.hpp"
)

# 手动设置 MySQL 库路径和头文件路径（根据实际情况修改）
set(MYSQL_INCLUDE_DIR /usr/include/mysql)
set(MYSQL_LIBRARIES /usr/lib/x86_64-linux-gnu/libmysqlclient.so)

# 包含头文件路径
include_directories(${PROJECT_SOURCE_DIR}/locker)
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${MYSQL_INCLUDE_DIR})

# 获取文件名（不带路径和扩展名）
set(MAIN ${PROJECT_SOURCE_DIR}/main.cpp)
get_filename_component(EXAMPLE_NAME ${MAIN} NAME_WE)

# 从源文件列表中移除 CMake 生成的文件夹
list(REMOVE_ITEM SRC ${PROJECT_SOURCE_DIR}/CMakeFiles/3.22.1/CompilerIdCXX/CMakeCXXCompilerId.cpp)

# 添加可执行文件
add_executable(${EXAMPLE_NAME} ${SRC})

# 链接库
target_link_libraries(${EXAMPLE_NAME} pthread Threads::Threads ${MYSQL_LIBRARIES})

# 定义编译选项
target_compile_definitions(${EXAMPLE_NAME} PRIVATE -DTHREADED)
