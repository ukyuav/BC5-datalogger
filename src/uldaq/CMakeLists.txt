cmake_minimum_required(VERSION 3.0)
project(uldaq)
add_compile_options(-std=c++11 -g)

file(GLOB SRC_FILES src/*.cpp  src/*/*.cpp  src/*/*/*.cpp 
                    src/*.h     src/*/*.h    src/*/*/*.h
                    src/*.c     src/*/*.c    src/*/*/*.c
  )

add_library(uldaq ${SRC_FILES})

target_link_libraries(uldaq
  usb-1.0
  )

