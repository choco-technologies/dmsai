set(DMOD_MODULE_NAME dmsai_port)
set(DMOD_AUTHOR_NAME "Patryk Kubiak")
set(DMOD_STACK_SIZE 1024)
set(DMOD_DMR_PATH ${CMAKE_SOURCE_DIR}/dmsai_port.dmr)

dmod_add_library(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    ${DMOD_CPU_FAMILY}/port.c
    ${DMSAI_PORT_COMMON_SOURCES}
)

target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(${DMOD_MODULE_NAME}_if INTERFACE
    ${CMAKE_SOURCE_DIR}/include
)

dmod_link_modules(${DMOD_MODULE_NAME}
    dmclk_port@>=1.2
)
