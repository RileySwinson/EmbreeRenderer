project(EmbreeRenderer)
cmake_minimum_required(VERSION 3.5)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Release) # optionally, debug

IF (NOT EMBREE_PROJECT_COMPILATION)
  FIND_PACKAGE(embree REQUIRED)
ENDIF()

# defaults for embree binaries:1

# backface_culling: OFF
# ignore_invalid_rays: OFF
# geometries: point, triangle, quad, curve, subdivision, user: ON
# geometry instances: ON
# max_instance_level: 1 (note: any further instances will DISSAPEAR in release mode)
# see readme for other defaults

# any deviations from defaults are documented in the readme

# end defaults


# should probably just move this to path
set(EMBREE_DIR "C:/Program Files/embree4")

include_directories("${EMBREE_DIR}/include")

link_directories("${EMBREE_DIR}/lib")

add_executable(EmbreeRenderer src/main.cpp) 

target_link_libraries(EmbreeRenderer embree4)

