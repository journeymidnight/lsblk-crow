cmake_minimum_required(VERSION 2.8)
project (crow_template)

find_package(Threads)
set(Boost_USE_STATIC_LIBS "On")
find_package(Boost 1.52 COMPONENTS system thread REQUIRED )



find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)


include_directories(include)
add_executable(serve-blk src/server.cpp src/lsblk.cpp)

SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11" )

target_link_libraries(serve-blk ${Boost_LIBRARIES})
target_link_libraries(serve-blk ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(serve-blk ${UDEV_LIBRARIES})
