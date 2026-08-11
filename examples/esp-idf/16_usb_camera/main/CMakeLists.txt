idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_board_manager
        gen_bmgr_codes
        s31_board_support
        esp_lcd
        esp_timer
        esp_psram
        heap
        lvgl__lvgl
        espressif__esp_new_jpeg
        espressif__usb
        espressif__usb_host_uvc
)
