cmake_minimum_required(VERSION 3.0)

project(uav)

add_compile_options(-g -Wall)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(vnproglib-1.1.4.0/cpp)
add_subdirectory(yaml-cpp)
add_subdirectory(uldaq)

add_executable(getData main.cpp) 
add_executable(extract extract.cpp)
add_executable(DAQ2CSV DAQ2CSV.cpp)
add_executable(vn_test vn_test.cpp)

target_compile_options(getData PUBLIC -O3) # compile the data aquisition to max optimization

include_directories(vnproglib-1.1.4.0/cpp/include yaml-cpp/include uldaq/src)

target_link_libraries(vn_test
	libvncxx
 ) 
target_link_libraries(getData
	yaml-cpp
	pthread
	wiringPi
	libvncxx
	uldaq
  stdc++fs
)

target_link_libraries(extract
	libvncxx
)

target_link_libraries(DAQ2CSV
	libvncxx
  yaml-cpp
)
install(TARGETS getData DESTINATION ../../bin)
install(TARGETS extract DESTINATION ../../bin)
install(TARGETS DAQ2CSV DESTINATION ../../bin)

