cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(maze_project)

idf_build_get_property(idf_target IDF_TARGET)
if(NOT idf_target STREQUAL "esp32c3")
    message(FATAL_ERROR "Este proyecto esta preparado para ESP32-C3. Ejecute: idf.py set-target esp32c3")
endif()
