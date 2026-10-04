set(DMOD_STACK_SIZE 4096)
dmod_add_executable(dmsai_hardware_test ${DMOD_MODULE_VERSION} hardware.c)
target_link_libraries(dmsai_hardware_test dmsai_if)
dmod_link_modules(dmsai_hardware_test dmheap)
