set(TARGET_NAME tcp_data_transfer)

add_executable(${TARGET_NAME}
        ${TARGET_NAME}.c
        tcp.c
        )
target_include_directories(${TARGET_NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${WIZNET_DIR}/Ethernet
)


target_link_libraries(${TARGET_NAME} PRIVATE
        pico_stdlib
        hardware_spi
        hardware_dma
        hardware_interp
        pico_multicore
        ETHERNET_FILES
        IOLIBRARY_FILES
        )

pico_enable_stdio_usb(${TARGET_NAME} 0)
pico_enable_stdio_uart(${TARGET_NAME} 1)

pico_add_extra_outputs(${TARGET_NAME})
