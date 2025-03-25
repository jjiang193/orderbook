# C++ High-Performance Limit Order Book

A high-performance limit order book implementation in C++ capable of handling over 1,400,000 transactions per second (TPS), including Market, Limit, Stop, and Stop Limit orders.

## Features

- **High Performance**: Optimized data structures and algorithms for maximum throughput
- **Thread-Safe**: All operations are protected with mutexes for concurrent access
- **Full Order Type Support**:
  - Market Orders: Execute immediately at the best available price
  - Limit Orders: Execute at a specific price or better
  - Stop Orders: Activate when the market price reaches a specified trigger level
  - Stop Limit Orders: Combine stop and limit order functionality
- **Order Management**:
  - Add new orders
  - Cancel existing orders
  - Modify order parameters (quantity, price, stop price)
- **FIFO and Price-Time Priority**: Orders are processed first by price, then by time of arrival

## Architecture

The matching engine uses a FIFO (First In, First Out) and Price Priority algorithm to determine the execution order of entries in the limit order book. The implementation is designed with the following components:

- **Order**: Base class for all order types
- **OrderBook**: Manages the order book and matching process
- **PriceLevel**: Represents a single price level with a queue of orders
- **Trade**: Represents a match between two orders

## How to Build

### Prerequisites

- CMake 3.10 or higher
- C++17 compatible compiler

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```

### Running the Example

```bash
./orderbook_main
```

### Running Performance Tests

```bash
./orderbook_main
```

This will run a performance benchmark with 100,000 random orders.

## Order Types

### Market Order

A market order is an order to buy or sell a financial instrument immediately at the current market price. Market orders guarantee execution but do not guarantee a specific price.

### Limit Order

A limit order is an order to buy or sell a financial instrument at a specific price or better. Buy limit orders are executed at the limit price or lower, and sell limit orders are executed at the limit price or higher.

### Stop Order

A stop order (also called a stop-loss order) is an order to buy or sell a financial instrument once the price reaches a specified price, known as the stop price. When the stop price is reached, the stop order becomes a market order.

### Stop Limit Order

A stop-limit order is a combination of a stop order and a limit order. Once the stop price is reached, the stop-limit order becomes a limit order to buy or sell at the limit price or better.

## Matching Algorithm

The order book uses a price-time priority algorithm for matching orders:

1. **Price Priority**: Orders with the best price are executed first
2. **Time Priority**: If multiple orders have the same price, the ones submitted earlier are executed first

For buy orders, the best price is the highest price. For sell orders, the best price is the lowest price.

## Performance Optimization

Several techniques are used to optimize performance:

1. **Efficient Data Structures**: 
   - Price levels are stored in ordered maps for quick lookup
   - Orders at each price level are stored in a linked list for efficient FIFO behavior
   - All orders are also stored in a hash map for O(1) lookup by ID

2. **Lock Granularity**: 
   - Separate mutexes for buy side, sell side, and order collections
   - Fine-grained locking to minimize contention

3. **Memory Management**: 
   - Smart pointers for automatic memory management
   - Minimal copying of objects

## License

This project is licensed under the MIT License - see the LICENSE file for details. 