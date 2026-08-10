idf_component_register(SRCS "main.c"
                    PRIV_REQUIRES esp_board_manager gen_bmgr_codes esp_codec_dev fatfs sdmmc
                    INCLUDE_DIRS ".")
