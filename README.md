# Dumb Task Scheduler

[![Build Status](https://github.com/karnkaul/dtasks/actions/workflows/ci.yml/badge.svg)](https://github.com/karnkaul/dtasks/actions/workflows/ci.yml)

This is a "dumb simple" multi-threaded task runner and scheduler library.

## Features

- Thread pool with configurable thread count
- Executor to enqueue and schedule tasks
  - Enqueue task, obtain future
  - Schedule a number of tasks after a number of associated dependencies (futures) have been completed (signalled)
- Rethrow the first exception caught, if any
- Stop/restart execution and queue at any time

## Dependencies

- [`ktl`](https://github.com/karnkaul/ktl) (via CMake FetchContent)

## Usage

### Requirements

- CMake
- C++20 compiler (and stdlib)

### Steps

1. Clone repo to appropriate subdirectory, say `dumb_tasks`
1. Add library to project via: `add_subdirectory(dumb_tasks)` and `target_link_libraries(foo dtasks::dtasks)`
1. Use via: `#include <dumb_tasks/executor.hpp>`

**Example**

```cpp
#include <dumb_tasks/executor.hpp>
#include <ktl/stack_string.hpp>
#include <iostream>
#include <vector>

int main() {
  dts::thread_pool pool;
  dts::executor executor(&pool);
  std::vector<dts::future_t> futures;
  int n{};
  for (int i = 0; i < 5; ++i) {
    auto future = executor.enqueue([n] { std::cout << ktl::stack_string<4>("{} ", n).get(); });
    futures.push_back(std::move(future));
    ++n;
  }
  n = 50;
  std::vector<dts::task_t> tasks;
  for (int i = 0; i < 5; ++i) {
    auto task = [n]() { std::cout << ktl::stack_string<4>("{} ", n).get(); };
    tasks.push_back(task);
    ++n;
  }
  auto future = executor.schedule(tasks, futures);
  future.wait(); 
  std::cout.flush();
  // potential output: 1 0 2 3 4 50 52 51 53 54
}
```

### Contributing

Pull/merge requests are welcome.
