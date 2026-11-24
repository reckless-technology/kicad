
add_library( core STATIC
    base64.cpp
    observable.cpp
    profile.cpp
    utf8.cpp
    version_compare.cpp
    wx_stl_compat.cpp
)

target_link_libraries( core PUBLIC
    ${wxWidgets_LIBRARIES}
)

target_include_directories( core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
    ${CMAKE_BINARY_DIR} # to get config.h
)