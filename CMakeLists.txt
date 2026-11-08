cmake_minimum_required(VERSION 3.20)
project(mqtt_counter_recovery LANGUAGES CXX)

find_package(GTest REQUIRED)
enable_testing()

add_executable(initiate_counter_test tests/initiate_counter_test.cpp)
target_compile_features(initiate_counter_test PRIVATE cxx_std_17)
target_include_directories(initiate_counter_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(initiate_counter_test PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(initiate_counter_test)
