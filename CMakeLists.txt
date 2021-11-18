cmake_minimum_required(VERSION 3.5)
project(package_gpc)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        pack_gpc.cpp
)

include_directories(./uvm/include ../jsondiff/jsondiff-cpp/include ../fc/include /usr/local/lib/boost/include)
link_directories(../fc /usr/local/lib/boost/lib)
link_libraries(fc boost_system boost_system boost_chrono boost_date_time boost_coroutine boost_context boost_thread boost_filesystem pthread)

add_executable(package_gpc ${SOURCE_FILES})
