cmake_minimum_required(VERSION 3.8)
project("dish")
set(CMAKE_CXX_STANDARD 17)
add_executable(dish "src/main.cpp" "src/dish.cpp" "src/builtin.cpp" "src/command.cpp" "src/error.cpp" "src/parser.cpp")
