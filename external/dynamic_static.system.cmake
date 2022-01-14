
# ==========================================
#   Copyright (c) 2021 dynamic_static
#       Licensed under the MIT license
#     http://opensource.org/licenses/MIT
# ==========================================

include(FetchContent)

if(EXISTS "${DYNAMIC_STATIC}/dynamic_static.system/")
    FetchContent_Declare(dynamic_static.system SOURCE_DIR "${DYNAMIC_STATIC}/dynamic_static.system/")
else()
    FetchContent_Declare(
        dynamic_static.system
        GIT_REPOSITORY "https://github.com/dynamic-static/dynamic_static.system.git"
        GIT_TAG 942c4a5cc09adb8b0d71a5d6af7b622fd1753ad3
        GIT_PROGRESS TRUE
        FETCHCONTENT_UPDATES_DISCONNECTED
    )
endif()
FetchContent_MakeAvailable(dynamic_static.system)
