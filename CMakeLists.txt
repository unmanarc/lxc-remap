cmake_minimum_required(VERSION 3.5)

project(lxc-remap LANGUAGES CXX)

INCLUDE(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.45.0 COMPONENTS filesystem)

if(Boost_FOUND)
    add_executable(lxc-remap main.cpp)
    target_link_libraries(lxc-remap Boost::filesystem)
    install(TARGETS lxc-remap
            RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR})
endif()
