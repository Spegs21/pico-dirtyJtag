pico_add_impl_library(drivers_dirtyJtag)

pico_generate_pio_header(drivers_dirtyJtag ${CMAKE_CURRENT_LIST_DIR}/jtag.pio)

target_sources(drivers_dirtyJtag INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/dirtyJtag.c
		${CMAKE_CURRENT_LIST_DIR}/dirtyJtag_cmd.c
		${CMAKE_CURRENT_LIST_DIR}/pio_jtag.c
)

target_include_directories(drivers_dirtyJtag INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(drivers_dirtyJtag INTERFACE
    hardware_pio
	hardware_dma
	tinyusb_device
	pico_multicore
)
