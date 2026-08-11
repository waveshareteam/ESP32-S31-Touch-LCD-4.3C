if(CONFIG_EXAMPLE_A2DP_SINK_USE_EXTERNAL_CODEC)
    set(EXTRA_COMPONENTS a2dp_sink_ext_codec_utils)
else()
    set(EXTRA_COMPONENTS a2dp_sink_esp_codec_dev_utils)
endif()

idf_component_register(SRCS "main.c"
                    PRIV_REQUIRES bt bt_app_core_utils bredr_app_common_utils a2dp_sink_common_utils music_ui esp_board_manager gen_bmgr_codes ${EXTRA_COMPONENTS}
                    INCLUDE_DIRS ".")
