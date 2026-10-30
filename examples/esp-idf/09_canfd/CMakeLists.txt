cmake_minimum_required(VERSION 3.22)

set(IDF_TARGET esp32s31)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(09_canfd)
