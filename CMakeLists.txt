cmake_minimum_required(VERSION 3.24)

project(WebServer)

aux_source_directory(. SRC_LIST) # 查找指定目录下的所有源文件, 然后存进指定变量名

# 将指定目录添加到编译器的头文件搜索路径之下
include_directories("${PROJECT_SOURCE_DIR}/httpmessage")
include_directories("${PROJECT_SOURCE_DIR}/httpevent")
include_directories("${PROJECT_SOURCE_DIR}/httpsession")
include_directories("${PROJECT_SOURCE_DIR}/logger")
include_directories("${PROJECT_SOURCE_DIR}/threadpool")
include_directories("${PROJECT_SOURCE_DIR}/utils")

# 子目录添加到构建系统中, 子目录有makelists.txt
add_subdirectory(httpmessage)
add_subdirectory(httpevent)
add_subdirectory(httpsession)
add_subdirectory(logger)
add_subdirectory(utils)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(main ${SRC_LIST})

# 添加链接库, 将 test 目录中的源文件编译为静态链接库
target_link_libraries(main http_event;http_msg;logger;http_session;utils)


