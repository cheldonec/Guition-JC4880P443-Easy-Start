idf_component_register(
    SRCS
        "board_init.c"
        "main.c"
        "main_sys_event_handler.c"
        "onboard_hardware/i2c_if/init_i2c.c"
        "onboard_hardware/i2s_if/init_i2s.c"
        "onboard_hardware/audio_module/init_es8311.c"
        "onboard_hardware/screen/lcd_st7701.c"
        "onboard_hardware/touch/touch_gt911.c"
        "onboard_hardware/storage/sd_card_io.c"
        "onboard_hardware/net_if/init_netif.c"
        
        "project_event_handler_manager/project_event_handler_manager.c"
        
        "lvgl_gui_app/init_lvgl_9.c"

        #WIFI_APPLICATION
        #"applications/sys_app_wifi_manager/wifi_manager.c"
        "applications/sys_app_wifi_manager/wifi_start_sta.c"
        "applications/sys_app_wifi_manager/wifi_manager_app.c"
        "applications/sys_app_wifi_manager/wifi_manager_scan_and_print.c"

        #TIME_SYSNC_APPLICATION
        "applications/sys_app_time_sync/time_sync_app.c"

        #MEMORY_MONITOR_APPLICATION
        "applications/sys_app_memory_monitor/memory_monitor_app.c"

        #ONBOARD_RCP_C6_CHIP_UPDATE HOSTED APPLICATION
        "applications/sys_app_rcp_c6_update/rcp_c6_update_app.c"
        "applications/sys_app_rcp_c6_update/ota_partition.c"

        #SD_CARD_INFO_APPLICATION
        "applications/sys_app_sd_card_info/sd_card_info_app.c"

        #SYS_APPLICATIONS_MANAGER
        "applications/sys_applications.c"

        #SYS_AUDIO_PLAY_AND_REC_APPLICATION
        "applications/sys_app_audio_play_and_rec/audio_app.c"
        "applications/sys_app_audio_play_and_rec/audio_rec.c"
        "applications/sys_app_audio_play_and_rec/audio_play.c"
        "applications/sys_app_audio_play_and_rec/audio_app_sample_events.c"

        #USER applications
        "applications_for_user/lvgl_audio_player/audio_player_app.c"

        
        #"applications/ya_cloud_speech_recognition/cloud_yandex.c"
        #"applications/ya_cloud_speech_recognition/ya_cloud_speech_recognition_app.c"
        
        
    INCLUDE_DIRS "."
    #EMBED_TXTFILES yandex-ca.pem
)

# === Настройка OTA через Partition (ESP-Hosted) ===
# 1. Убедиться, что папка slave_fw_bin существует
set(SLAVE_FW_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/applications/sys_app_rcp_c6_update/slave_fw_bin")
file(MAKE_DIRECTORY "${SLAVE_FW_BIN_DIR}")

# 2. Копировать network_adapter.bin в slave_fw_bin/ как 1_network_adapter.bin
set(SOURCE_BIN "${CMAKE_CURRENT_SOURCE_DIR}/../network_adapter.bin")
set(TARGET_BIN "${SLAVE_FW_BIN_DIR}/1_network_adapter.bin")

if(EXISTS "${SOURCE_BIN}")
    add_custom_target(
        copy_network_adapter_bin
        COMMAND ${CMAKE_COMMAND} -E copy
            "${SOURCE_BIN}"
            "${TARGET_BIN}"
        COMMENT "📋 Copying network_adapter.bin → slave_fw_bin/1_network_adapter.bin (always overwrite)"
        VERBATIM
    )
    add_dependencies(${COMPONENT_LIB} copy_network_adapter_bin)
    message(STATUS "✅ OTA setup: Copied network_adapter.bin to ${TARGET_BIN}")
else()
    message(WARNING "⚠️ OTA setup: network_adapter.bin not found at ${SOURCE_BIN}")
    message(WARNING "    Expected: ${SOURCE_BIN}")
    message(WARNING "    Create it or set CONFIG_OTA_METHOD_PARTITION=n")
endif()

# === flash_slave_fw: прошивает 1_network_adapter.bin в раздел slave_fw ===
# Используем фиксированный offset 0x620000 — это precisely what IDF calculates:
#   ota_1: 0x320000 + 0x300000 = 0x620000 → slave_fw starts here
set(SLAVE_FW_OFFSET "0x620000")
message(STATUS "✅ flash_slave_fw: Using fixed offset 0x620000 for slave_fw (IDF calculated)")

# Создаём цель flash_slave_fw, если файл существует
if(EXISTS "${TARGET_BIN}")
    add_custom_target(
        flash_slave_fw
        COMMAND ${CMAKE_COMMAND} -E echo "🔧 Flashing slave firmware..."
        COMMAND ${CMAKE_COMMAND} -E echo "   File: ${TARGET_BIN}"
        COMMAND ${CMAKE_COMMAND} -E echo "   Partition: slave_fw @ offset ${SLAVE_FW_OFFSET}"
        COMMAND python -m esptool --port ${IDF_PY_PORT} write_flash --force ${SLAVE_FW_OFFSET} "${TARGET_BIN}"
        DEPENDS ${TARGET_BIN}
        COMMENT "🔥 Flashing network_adapter.bin to slave_fw partition"
        VERBATIM
    )
    message(STATUS "✅ flash_slave_fw target created: idf.py flash_slave_fw")
else()
    message(WARNING "⚠️ Not creating flash_slave_fw target: ${TARGET_BIN} not found")
    message(WARNING "    Run: idf.py build (to copy network_adapter.bin)")
endif()