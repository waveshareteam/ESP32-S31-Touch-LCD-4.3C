idf_component_register(
    SRCS "s31_board_support.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_board_manager gen_bmgr_codes esp_driver_gpio esp_driver_i2c esp_driver_twai
)
