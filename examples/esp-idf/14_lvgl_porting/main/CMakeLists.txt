idf_component_register(SRCS "main.c" "app_media.c" "app_ui.c"
                    PRIV_REQUIRES esp_board_manager gen_bmgr_codes esp_lcd esp_lvgl_adapter lvgl__lvgl esp_timer esp_mm heap
                    INCLUDE_DIRS ".")
