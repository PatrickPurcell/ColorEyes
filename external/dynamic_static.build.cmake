
# ==========================================
#   Copyright (c) 2020-2021 dynamic_static
#       Licensed under the MIT license
#     http://opensource.org/licenses/MIT
# ==========================================

include(FetchContent)

if(EXISTS "${DYNAMIC_STATIC}/dynamic_static.build/")
    FetchContent_Declare(dynamic_static.build SOURCE_DIR "${DYNAMIC_STATIC}/dynamic_static.build/")
else()
    FetchContent_Declare(
        dynamic_static.build
        GIT_REPOSITORY "https://github.com/dynamic-static/dynamic_static.build.git"
        GIT_TAG c67ee23b538880cb2027dd1e73dad41a8071bbce
        GIT_PROGRESS TRUE
        FETCHCONTENT_UPDATES_DISCONNECTED
    )
endif()
FetchContent_MakeAvailable(dynamic_static.build)
include("${dynamic_static.build_SOURCE_DIR}/dynamic_static.build.cmake")
