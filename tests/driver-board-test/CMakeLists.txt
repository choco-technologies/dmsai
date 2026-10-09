set(DMOD_MODULE_NAME dmsai_driver_board_test)
set(DMOD_AUTHOR_NAME "Patryk Kubiak")
set(DMOD_STACK_SIZE 2048)
dmod_add_executable(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    driver_board_test.c)
target_include_directories(${DMOD_MODULE_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include)
dmod_link_modules(${DMOD_MODULE_NAME} dmdrvi)
