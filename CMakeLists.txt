cmake_minimum_required(VERSION 3.16)
project(BluetoothAoaLocator C)

set(CMAKE_C_STANDARD 99)

add_executable(BluetoothAoaLocator system.h
        app_assert.h
        app_log.h
        sl_rtl_clib_api.h
        sl_bt_ncp_host.h
        conn.h
        app_assert_config.h
        mqtt.c
        ncp_host_nothread.c
        app.c
        sl_status.h
        aoa_config.h
        app_log_cli.c
        mqtt.h
        aoa_types.h
        main.c
        aoa_serdes.c
        cJSON.h
        aoa_serdes.h
        tcp_posix.c
        aoa_angle.c
        aoa_util.h
        aoa_util.c
        aoa_board.h
        sli_bt_api.h
        aoa_parse.h
        conn.c
        aoa_angle.h
        app_log_cli.h
        app_log.c
        sl_iostream_handles.c
        app_signal.h
        cJSON.c
        sl_ncp_evt_filter_common.h
        aoa_angle_config.h
        ncp_host.h
        sl_bt_ncp_host_api.c
        sl_bgapi.h
        aoa_config.c
        sl_bt_api_compatibility.h
        sl_bt_api.h
        sl_iostream_handles.h
        sl_bt_ncp_host.c
        uart_posix.c
        app_silabs.c
        app_signal_posix.c
        system.c
        app_log_config.h
        app.h
        app_config.h
        sl_iostream.h
        aoa_parse.c
        uart.h
        tcp.h)
target_link_libraries(BluetoothAoaLocator -lm -lstdc++ -lpthread -lmosquitto ${CMAKE_SOURCE_DIR}/libaox_static.a)
add_definitions(-DPOSIX -DHOST_TOOLCHAIN -DAPP_LOG_NEW_LINE=APP_LOG_NEW_LINE_RN -DSL_CATALOG_APP_LOG_PRESENT -DAOA_ANGLE -DRTL_LIB)
add_compile_options(-fno-short-enums -Wall -fmessage-length=0 -MMD -MP -MF -Wno-misleading-indentation -O0 -g3)