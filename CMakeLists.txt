file(GLOB_RECURSE APPLICATION ./ *.cpp)

add_library(application ${APPLICATION}    "core.h"  "core.cpp")