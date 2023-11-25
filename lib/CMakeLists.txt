add_library(mylib mylib.c mylib.h rio.c rio.h)

target_include_directories(mylib
                           INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
                           )