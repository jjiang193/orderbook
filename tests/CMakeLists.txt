find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Test executables
add_executable(order_tests OrderTests.cpp)
target_link_libraries(order_tests orderbook ${GTEST_LIBRARIES} pthread)

add_executable(orderbook_tests OrderBookTests.cpp)
target_link_libraries(orderbook_tests orderbook ${GTEST_LIBRARIES} pthread)

# Add tests
add_test(NAME order_tests COMMAND order_tests)
add_test(NAME orderbook_tests COMMAND orderbook_tests) 