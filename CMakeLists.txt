
# ==========================================
#   Copyright (c) 2016-2021 dynamic_static
#       Licensed under the MIT license
#     http://opensource.org/licenses/MIT
# ==========================================

cmake_minimum_required(VERSION 3.3 FATAL_ERROR)
project(ColorEyes VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 20)

################################################################################
# options
option(DYNAMIC_STATIC_SYSTEM_OPENGL_ENABLED "" ON)

################################################################################
# dependencies
set(external "${CMAKE_CURRENT_LIST_DIR}/external/")
string(REPLACE "\\" "/" DYNAMIC_STATIC "$ENV{DYNAMIC_STATIC}")
include("${external}/dynamic_static.build.cmake")
include("${external}/dynamic_static.system.cmake")

################################################################################
# ColorEyes
dst_add_executable(
    target ColorEyes
    folder ColorEyes
    linkLibraries "${linkLibraries}" dynamic_static.system
    sourceFiles "${CMAKE_CURRENT_LIST_DIR}/main.cpp"
)
