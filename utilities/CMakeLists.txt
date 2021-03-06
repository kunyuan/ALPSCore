# 
# alps-utilities cmake configuration script.
# 

cmake_minimum_required (VERSION 2.8.12)

# define project name
project (alps-utilities CXX)
message ("\nALPSCore module : ${PROJECT_NAME}\n")

# add common modules from ../common/cmake
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../common/cmake)
include(ALPSCommonModuleDefinitions)

gen_documentation()

if (DocumentationOnly)
  return()
endif()

include(CheckIncludeFile)

add_this_package(
    # copyright //FIXME
    os
    temporary_filename
    remove_extensions
    stacktrace
    signal
    gtest_par_xml_output
)

include(ALPSEnableMPI)

#if(ALPS_HAVE_MPI)
#  add_boost(filesystem system chrono program_options mpi)
#else()
add_boost(filesystem system chrono program_options)
#endif()

add_testing()
CHECK_INCLUDE_FILE(unistd.h ALPS_HAVE_UNISTD_H)
gen_hpp_config()
gen_pkg_config()
gen_cfg_module()
