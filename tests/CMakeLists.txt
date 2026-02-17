cmake_minimum_required(VERSION 3.16)

include(CTest)

set(TEST_SOURCES
    test_packets.c
    test_network_data_operations.c
    test_rx_packet_handler.c
)

foreach(src IN LISTS TEST_SOURCES)
    get_filename_component(test_name ${src} NAME_WE)
    add_executable(${test_name} ${src})
    target_link_libraries(${test_name} PRIVATE 
        lora_networking_lib
        m
    )

    target_compile_options(${test_name}
        PRIVATE -Wall -Wextra -Wpedantic -g -O0)

    add_test(NAME lora_networking_lib.${test_name} COMMAND ${test_name})
    set_tests_properties(lora_networking_lib.${test_name} PROPERTIES WORKING_DIRECTORY $<TARGET_FILE_DIR:${test_name}>)
endforeach()
