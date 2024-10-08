# DO NOT DELETE THIS LINE:: @archetype_spec@
SET(STUB_SOURCE_DIR ${CMAKE_SOURCE_DIR})

EXECUTE_PROCESS(COMMAND cyclus --install-path 
    OUTPUT_VARIABLE CYCLUS_INSTALL_PATH 
    OUTPUT_STRIP_TRAILING_WHITESPACE)
MESSAGE("CYCLUS_INSTALL_PATH: ${CYCLUS_INSTALL_PATH}")

# Tell CMake where the modules are
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CYCLUS_INSTALL_PATH}/share/cyclus/cmake")
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${STUB_SOURCE_DIR}/cmake)
MESSAGE("--CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}")

INCLUDE(CyclusBuildSetup)
cyclus_minimum_cmake_version(MINIMUM_CMAKE_VERSION)
CMAKE_MINIMUM_REQUIRED(VERSION ${MINIMUM_CMAKE_VERSION})

PROJECT(@lib_name@)

cyclus_require_out_of_source_build()
cyclus_require_cxx_support()

# quiets fortify_source warnings when not compiling with optimizations
# in linux distros where compilers were compiled with fortify_source enabled by
# default (e.g. Arch linux).
MESSAGE("${CMAKE_BUILD_TYPE}")
STRING(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE)
IF(NOT ${BUILD_TYPE} STREQUAL "release")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0")
ENDIF()
# no overflow warnings because of silly coin-ness
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overflow")

# Direct any binary installation paths to this directory
SET(STUB_BINARY_DIR ${CMAKE_BINARY_DIR})

# This makes all the libraries build as SHARED
SET(BUILD_SHARED_LIBS true)

# Setup build locations.
cyclus_setup_build_locations(${STUB_BINARY_DIR})

SET(STUB_EXECUTABLE_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

cyclus_set_rpath()

# Find cyclus
FIND_PACKAGE(Cyclus REQUIRED)
SET(STUB_INCLUDE_DIRS ${STUB_INCLUDE_DIRS} ${CYCLUS_CORE_INCLUDE_DIR})
SET(LIBS ${LIBS} ${CYCLUS_CORE_LIBRARIES})

# Include macros
INCLUDE(UseCyclus)

MESSAGE("--LD_LIBRARY_PATH: $ENV{LD_LIBRARY_PATH}")

# Include the boost header files, system, and filesystem libraries
SET(Boost_USE_STATIC_LIBS       OFF)
SET(Boost_USE_STATIC_RUNTIME    OFF)
FIND_PACKAGE(Boost COMPONENTS filesystem system REQUIRED)
SET(STUB_INCLUDE_DIRS ${STUB_INCLUDE_DIRS} ${Boost_INCLUDE_DIR})
SET(LIBS ${LIBS} ${Boost_FILESYSTEM_LIBRARY})
SET(LIBS ${LIBS} ${Boost_SYSTEM_LIBRARY})

# Find HDF5
FIND_PACKAGE(HDF5 REQUIRED)
ADD_DEFINITIONS(${HDF5_DEFINITIONS})
SET(STUB_INCLUDE_DIRS ${STUB_INCLUDE_DIRS} ${HDF5_INCLUDE_DIR})
set(LIBS ${LIBS} ${HDF5_LIBRARIES})
MESSAGE("--    HDF5 Root: ${HDF5_ROOT}")
MESSAGE("--    HDF5 Include directory: ${HDF5_INCLUDE_DIR}")
MESSAGE("--    HDF5 Library directories: ${HDF5_LIBRARY_DIRS}")
MESSAGE("--    HDF5 Libraries: ${HDF5_LIBRARIES}")

# find Coin
FIND_PACKAGE(COIN REQUIRED)
SET(STUB_INCLUDE_DIRS ${STUB_INCLUDE_DIRS} ${COIN_INCLUDE_DIR})
set(LIBS ${LIBS} ${COIN_LIBRARIES})


# include all the directories we just found
INCLUDE_DIRECTORIES(${STUB_INCLUDE_DIRS})


# add the agents
ADD_SUBDIRECTORY(src)

# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
