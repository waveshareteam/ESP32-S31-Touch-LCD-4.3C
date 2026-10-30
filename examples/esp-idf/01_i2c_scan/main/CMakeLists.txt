idf_component_register(SRCS "main.c"
                    PRIV_REQUIRES waveshare_boards gen_bmgr_codes esp_driver_i2c esp_driver_gpio
                    INCLUDE_DIRS ".")
