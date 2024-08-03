cmake_minimum_required(VERSION 3.10)

# 项目信息
project(webserver)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)

# 设置输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 库名称
set(LIBRARY_NAME webserver)

# 查找线程库
find_package(Threads REQUIRED)

# 查找主目录下的所有 .cpp 文件，但排除 CMake 自己生成的文件夹
file(GLOB_RECURSE SRC
    "${PROJECT_SOURCE_DIR}/*.cpp"
)

# 手动设置 MySQL 库路径和头文件路径（根据实际情况修改）
set(MYSQL_INCLUDE_DIR /usr/include/mysql)
set(MYSQL_LIBRARIES /usr/lib/x86_64-linux-gnu/libmysqlclient.so)

# 查找 hiredis 库
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARIES hiredis)

# 包含头文件路径
include_directories(${PROJECT_SOURCE_DIR}/locker)
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${MYSQL_INCLUDE_DIR})
include_directories(${HIREDIS_INCLUDE_DIR})

# 获取文件名（不带路径和扩展名）
set(MAIN ${PROJECT_SOURCE_DIR}/main.cpp)
get_filename_component(EXAMPLE_NAME ${MAIN} NAME_WE)

# 从源文件列表中移除 CMake 生成的文件夹
list(REMOVE_ITEM SRC ${PROJECT_SOURCE_DIR}/CMakeFiles/3.22.1/CompilerIdCXX/CMakeCXXCompilerId.cpp)
list(REMOVE_ITEM SRC ${MAIN})

# 添加共享库
add_library(${LIBRARY_NAME} SHARED ${SRC})
target_compile_definitions(${LIBRARY_NAME} PRIVATE -DTHREADED)
target_link_libraries(${LIBRARY_NAME} pthread Threads::Threads ${MYSQL_LIBRARIES} ${HIREDIS_LIBRARIES})

# 添加可执行文件
add_executable(${EXAMPLE_NAME} ${MAIN})
target_compile_definitions(${EXAMPLE_NAME} PRIVATE -DTHREADED)

# 设置库路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 链接库
target_link_libraries(${EXAMPLE_NAME} ${LIBRARY_NAME} pthread Threads::Threads ${MYSQL_LIBRARIES} ${HIREDIS_LIBRARIES})
