cmake_minimum_required (VERSION 3.2.0)

project (resLoadSIM
         VERSION 5.4.0
         LANGUAGES CXX)

#----------------------------------------------------------------------#
# Find the sources, create a target and set the path to the headers    #
#----------------------------------------------------------------------#

file (GLOB SOURCES src/*.cpp)
add_executable (${PROJECT_NAME} ${SOURCES})
target_include_directories (${PROJECT_NAME} PRIVATE include)

#----------------------------------------------------------------------#
# Create a header file named 'version.H' containing the version nr     #
#----------------------------------------------------------------------#

configure_file (include/version.H.in ${CMAKE_CURRENT_BINARY_DIR}/version.H)
target_include_directories (${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

#----------------------------------------------------------------------#
# Do we want to build the debug version?                               #
#----------------------------------------------------------------------#

if (NOT WIN32)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        message (STATUS "Build the DEBUG version")
        target_compile_definitions (${PROJECT_NAME} PRIVATE DEBUG)
        target_compile_options (${PROJECT_NAME} PRIVATE -g2 -pedantic -Wall -Wextra)
    else()
        message (STATUS "Build the OPTIMIZED version")
        target_compile_options (${PROJECT_NAME} PRIVATE -O3 -Wno-unused-result)
    endif()
endif ()

#----------------------------------------------------------------------#
# Check, whether the user wants to build the parallel version.         #
# If this is the case, look for MPI:                                   #
#----------------------------------------------------------------------#

option (PARALLEL "build the parallel version" OFF)
if (PARALLEL)
    find_package (MPI REQUIRED)
    if (MPI_CXX_FOUND)
        target_include_directories (${PROJECT_NAME} PRIVATE ${MPI_CXX_INCLUDE_PATH})
        target_compile_definitions (${PROJECT_NAME} PRIVATE PARALLEL)
        target_compile_options (${PROJECT_NAME} PRIVATE ${MPI_CXX_COMPILE_FLAGS})
        target_link_libraries (${PROJECT_NAME} ${MPI_CXX_LINK_FLAGS} ${MPI_CXX_LIBRARIES})
    else()
        message (FATAL_ERROR "\nIt seems that MPI is not installed on your system")
    endif()
endif()

#----------------------------------------------------------------------#
# Look for a powerflow solver which comes with the PETSc library.      #
# In older versions of PETSc it was called 'pf', now it's called       #
# 'power'.                                                             #
#----------------------------------------------------------------------#

find_program (PF pf)
if (PF)
    message (STATUS "Found pf")
    target_compile_definitions (${PROJECT_NAME} PRIVATE HAVE_PF)
else()
    find_program (POWER power)
    if (POWER)
        message (STATUS "Found power")
        target_compile_definitions (${PROJECT_NAME} PRIVATE HAVE_POWER)
    endif()
endif()

#----------------------------------------------------------------------#
# cURL is used to download solar radiation data from the PVGIS         #
# database. This one is optional.                                      #
#----------------------------------------------------------------------#

find_package (CURL)
if (CURL_FOUND)
    target_link_libraries (${PROJECT_NAME} ${CURL_LIBRARIES})
    target_include_directories (${PROJECT_NAME} PRIVATE ${CURL_INCLUDE_DIRS})
    target_compile_definitions (${PROJECT_NAME} PRIVATE HAVE_CURL)
endif()

#----------------------------------------------------------------------#
# Generate the installation rule                                       #
#----------------------------------------------------------------------#

install (TARGETS ${PROJECT_NAME}
         RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

#----------------------------------------------------------------------#
# Packaging                                                            #
#----------------------------------------------------------------------#

set (CPACK_PACKAGE_VENDOR "European Union")
set (CPACK_SOURCE_GENERATOR "TGZ")
set (CPACK_SOURCE_IGNORE_FILES
     /Arbeit*
     /build*
     /CVS
     /.cvsignore
     /.vscode
     /delta
     /profile
     /FlowCharts
     /Manual.odf
     /*tar.gz
)
include (CPack)
