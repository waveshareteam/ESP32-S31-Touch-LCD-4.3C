idf_component_register(SRCS "main.c"
                         "ui.c"
                         "test_lcd_touch.c"
                         "test_wifi.c"
                         "test_rtc.c"
                         "test_imu.c"
                         "test_bq27220.c"
                         "test_tf_card.c"
                         "test_rs485_canfd_bridge.c"
                         "test_audio.c"
                         "test_usb_host.c"
                    PRIV_REQUIRES esp_board_manager gen_bmgr_codes s31_board_support
                                  esp_lcd esp_lvgl_adapter lvgl__lvgl esp_wifi esp_event esp_netif nvs_flash
                                  esp_driver_uart esp_driver_twai esp_timer esp_codec_dev
                                  espressif__usb heap
                    INCLUDE_DIRS ".")
