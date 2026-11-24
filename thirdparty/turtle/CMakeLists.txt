add_library( turtle INTERFACE )

# Reckless: vendored code. As a SYSTEM directory its headers' warnings are not ours to fix
target_include_directories( turtle SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} )
