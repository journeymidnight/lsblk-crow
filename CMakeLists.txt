cmake_minimum_required(VERSION 2.8)
project (serve-blk)

find_package(Threads)
set(Boost_USE_STATIC_LIBS "On")
find_package(Boost 1.52 COMPONENTS system thread REQUIRED )



find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)
pkg_check_modules(MOUNT REQUIRED mount)


include_directories(include)
add_executable(serve-blk src/server.cpp src/lsblk.cpp src/path.c)

SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g" )

target_link_libraries(serve-blk ${Boost_LIBRARIES})
target_link_libraries(serve-blk ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(serve-blk ${UDEV_LIBRARIES})
target_link_libraries(serve-blk ${MOUNT_LIBRARIES})
target_link_libraries(serve-blk pthread)

set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
set(CMAKE_CURRENT_BINARY_DIR ..)
install(PROGRAMS build/serve-blk DESTINATION bin)
install(FILES package/serve-blk.service DESTINATION lib/systemd/system)

set(CPACK_PACKAGE_NAME "serve-blk")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Simple CPack HelloWorld")
set(CPACK_PACKAGE_VENDOR "WeTest")
set(CPACK_PACKAGE_VERSION "1.0.1")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "1")
set(CPACK_RPM_PACKAGE_GROUP "WeTest")
set(CPACK_RPM_PACKAGE_URL "http://wetest.qq.com")
set(CPACK_RPM_PACKAGE_DESCRIPTION "WeTest Server Dependencies")
set(CPACK_PACKAGE_RELEASE 1)
set(CPACK_RPM_PACKAGE_LICENSE "WeTest Licence")

set(CPACK_GENERATOR "RPM")

set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/package/post_script.sh)
include(CPack)
