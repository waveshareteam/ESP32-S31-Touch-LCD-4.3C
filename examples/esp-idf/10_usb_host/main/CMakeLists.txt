idf_component_register(SRCS "main.c"
                    PRIV_REQUIRES esp_board_manager gen_bmgr_codes s31_board_support espressif__usb
                    INCLUDE_DIRS ".")
