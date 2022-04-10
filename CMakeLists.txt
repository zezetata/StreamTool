cmake_minimum_required(VERSION 3.21)
project(StreamTool)

set(CMAKE_CXX_STANDARD 14)

add_executable(StreamTool main.cpp h264_sei_wrapper.cpp h264_sei_wrapper.h h265_sei_wrapper.cpp h265_sei_wrapper.h)
