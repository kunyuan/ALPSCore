### gftools cmake configuration

cmake_minimum_required (VERSION 2.8.12)

# gftools use only CXX compiler 
project (alps-gf CXX)
message ("\nALPSCore module : ${PROJECT_NAME}\n")

# find additional cmake module in cmake dir
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../common/cmake)
include(ALPSCommonModuleDefinitions)

gen_documentation()
if (DocumentationOnly)
  return()
endif()

add_this_package(mesh gf)
include(ALPSEnableMPI)

add_boost()
add_hdf5()
add_alps_package(alps-utilities alps-hdf5)
add_testing()

gen_cfg_module()
gen_pkg_config()
    
