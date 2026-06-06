cmake_minimum_required(VERSION 4.2)
project(zigbee_button)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(zigbee_button
        button.cpp
        button.h
        button_monitor.cpp
        button_monitor.h
        led_indicator.cpp
        led_indicator.h
        settings.cpp
        settings.h
        version.h
        zigbee_button.ino
        zigbee_handler.cpp
        zigbee_handler.h)
