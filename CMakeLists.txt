cmake_minimum_required(VERSION 3.10)
project(OrderBook VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler flags for optimization and warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /O2")
endif()

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/src)

# Library source files
set(LIB_SOURCES
    src/Order.cpp
    src/OrderBook.cpp
)

# Create library
add_library(orderbook STATIC ${LIB_SOURCES})

# Main executable
add_executable(orderbook_main src/main.cpp)
target_link_libraries(orderbook_main orderbook)

# If tests are enabled
option(BUILD_TESTS "Build the tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS orderbook orderbook_main
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY src/include/orderbook
        DESTINATION include)

# Packaging
set(CPACK_PACKAGE_NAME "OrderBook")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance order book for financial trading")
include(CPack) 