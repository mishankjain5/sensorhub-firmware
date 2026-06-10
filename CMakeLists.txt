cmake_minimum_required(VERSION 3.16)
project(sensorhub C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra)

# Portable application logic — compiles for both the MCU and the host.
set(APP_SOURCES
    app/ringbuf.c
    app/fmt.c
    app/ema.c
    app/alarm.c
    app/stats.c
    app/sampler.c
    app/shell.c
)

if(CMAKE_CROSSCOMPILING)
    # ---- Firmware image for the LM3S6965 (run in QEMU) ----
    add_executable(sensorhub.elf
        ${APP_SOURCES}
        target/startup.c
        target/uart.c
        target/systick.c
        target/main.c
    )
    target_link_options(sensorhub.elf PRIVATE
        -T${CMAKE_SOURCE_DIR}/target/lm3s6965.ld
        -nostartfiles
        -Wl,-Map=sensorhub.map
    )
    # Show flash/RAM usage after every build — embedded devs watch this.
    add_custom_command(TARGET sensorhub.elf POST_BUILD
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:sensorhub.elf>
    )
else()
    # ---- Host build: unit tests for the portable app/ modules ----
    enable_testing()
    add_executable(sensorhub_tests
        ${APP_SOURCES}
        tests/test_main.c
    )
    add_test(NAME unit_tests COMMAND sensorhub_tests)
endif()
