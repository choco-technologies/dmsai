# ======================================================================
#               dmsai_port Module Configuration
# ======================================================================

# Name of the module
set(DMOD_MODULE_NAME        dmsai_port)

# Version is already set above and used in project()

# Author (should be string)
set(DMOD_AUTHOR_NAME        "Patryk Kubiak")

# Stack size for the module (should be integer)
set(DMOD_STACK_SIZE         1024)

# Path to the DMR file for this module
set(DMOD_DMR_PATH           ${CMAKE_SOURCE_DIR}/dmsai_port.dmr)

#
#   dmod_add_library - create a library module
#   it has the same signature as add_library
#   and can be used in the same way after the creation
#   (for example, to link libraries)
#
dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    # List of source files - can include C and C++ files
    ${DMOD_CPU_FAMILY}/port.c
)

# To depend on another module's headers, use:
#   dmod_link_modules(${DMOD_MODULE_NAME}
#       other_module@1.0
#   )

target_include_directories(${DMOD_MODULE_NAME} BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(${DMOD_MODULE_NAME}_if INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

# dmclk SAI API is supplied by the companion dmclk PR until its release.
dmod_link_modules(dmsai_port "dmclk@>=1.2" dmdma)
set(DMSAI_DMCLK_INCLUDE_DIR "" CACHE PATH "Headers from a dmclk build containing the SAI clock API")
if(DMSAI_DMCLK_INCLUDE_DIR)
    target_include_directories(dmsai_port BEFORE PRIVATE ${DMSAI_DMCLK_INCLUDE_DIR})
endif()
