cmake_minimum_required(VERSION 3.30)
project(AutoTaskbar)

set(CMAKE_CXX_STANDARD 20)
add_definitions(-DUNICODE -D_UNICODE)

set(RESOURCE_FILES resources.rc)

add_executable(${PROJECT_NAME} WIN32
        main.cpp
        TaskbarManager.cpp
        TaskbarManager.h
        SystemTray.cpp
        SystemTray.h
        ${RESOURCE_FILES}
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            comctl32.lib
            shell32.lib
            user32.lib
            gdi32.lib
            advapi32.lib
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE comctl32 shell32 user32 gdi32 advapi32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "-static -static-libgcc -static-libstdc++"
    )
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
set(CPACK_PACKAGE_NAME "AutoTaskbar")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VENDOR "Liu")
include(CPack)
