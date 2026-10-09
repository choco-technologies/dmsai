set(DMOD_MODULE_NAME dmsai_port_board_test)
set(DMOD_AUTHOR_NAME "Patryk Kubiak")
set(DMOD_STACK_SIZE 2048)
dmod_add_executable(${DMOD_MODULE_NAME} ${DMOD_MODULE_VERSION}
    port_board_test.c)
target_link_libraries(${DMOD_MODULE_NAME} dmsai_port_if)
dmod_link_modules(${DMOD_MODULE_NAME} dmclk_port@>=1.2)
