cmake_minimum_required(VERSION 3.26)
project(main)

find_package(Arrow REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(a.out main.cpp)
find_package(Parquet CONFIG REQUIRED PATHS ${Arrow_DIR} NO_DEFAULT_PATH)
include_directories(${OpenCV_INCLUDE_DIRS} )

target_link_libraries(a.out ${OpenCV_LIBS} )

target_link_libraries(a.out arrow)
target_link_libraries(a.out parquet)
