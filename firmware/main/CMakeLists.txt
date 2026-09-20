idf_component_register(SRCS "main.cpp" "board.cpp" "radio.cpp" "storage.cpp"
  "../core/market.cpp" "../core/session.cpp" "../core/play.cpp" "../core/bomb.cpp"
  INCLUDE_DIRS "." "../core"
  REQUIRES esp_lcd esp_driver_gpio esp_driver_spi esp_driver_i2c esp_driver_rmt
  esp_driver_usb_serial_jtag esp_timer esp_wifi esp_event esp_netif nvs_flash)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
