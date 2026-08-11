idf_component_register(SRCS "a2dp_sink_esp_codec_dev_utils.c"
                    PRIV_REQUIRES bt esp_ringbuf esp_driver_i2s esp_codec_dev esp_board_manager
                    INCLUDE_DIRS ".")
