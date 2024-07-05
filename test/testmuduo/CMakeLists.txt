cmake_minimum_required(VERSION 3.0)
project(main)

#配置编译选项
# set(CMAKE_BUILD_TYPE "Debug")#设置debug模式，如果没有这一行将不能调试设断点
# set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

#配置头文件的搜索路径
#include_directories()
#配置库文件的搜索路径
#link_directories()

#设置需要编译的源文件列表
set(SRC_LIST muduo_server.cpp)


#设置可执行文件最终存储的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

#把指定路径下的所有源文件名字放入变量名SRC_LIST中
# aux_source_directory(./ SRC_LISTS)

add_executable(server ${SRC_LIST}) 
#表示server这个目标程序需要链接 muduo_net muduo_base pthread
target_link_libraries(server muduo_net muduo_base pthread)