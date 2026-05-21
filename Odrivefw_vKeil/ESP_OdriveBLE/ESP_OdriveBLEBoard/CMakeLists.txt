# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# 设置自定义分区表
set(PARTITION_CSV_PATH ${CMAKE_SOURCE_DIR}/partitions.csv)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(P169H002)

# GCC ICE workaround: esp_lcd rgb_panel_draw_bitmap triggers a segfault in the
# register allocator (IRA pass) at -O2/-Os. Lower esp_lcd to -O1 to avoid it.
target_compile_options(__idf_esp_lcd PRIVATE -O1)