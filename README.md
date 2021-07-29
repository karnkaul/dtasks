## Dumb Task Scheduler

[![Build status](https://ci.appveyor.com/api/projects/status/kgxf74qofu7mfc92?svg=true)](https://ci.appveyor.com/project/karnkaul/dtasks)

This is a "dumb simple" multi-threaded task runner and scheduler library.

### Features

- Task queue to push flat callables into
  - Configurable thread / worker count
  - Async queue using condition variable: workers sleep as long as queue remains empty
  - Task ID per enqueued task, can be used to poll status / wait for
  - `#define DTASKS_CATCH_RUNTIME_ERRORS` to enable runtime error handling (on by default in `Debug`)
  - Customizable error handler (logs `what()` to `std::cerr` by default)
- Task scheduler (derived class) to stage batches of tasks, optionally with dependencies (pushed stages)
  - Stage ID per batch, can be used to poll status / wait for

### Dependencies

- [`ktl`](https://github.com/karnkaul/ktl) (via CMake FetchContent)

### Usage

**Requirements**

- CMake
- C++17 compiler (and stdlib)

**Steps**

1. Clone repo to appropriate subdirectory, say `dumb_tasks`
1. Add library to project via: `add_subdirectory(dumb_tasks)` and `target_link_libraries(foo dtasks::dtasks)`
1. Use via: `#include <dumb_tasks/scheduler.hpp>` or `#include <dumb_tasks/task_queue.hpp>` (if scheduling / dependencies are not needed)

**Example**

`dts::task_queue` is a flat async queue of tasks which are consumed by a fixed number of worker threads.

`dts::scheduler` derives from `dts::task_queue` and provides the ability to enqueue batches of tasks in a "stage", with optional dependencies (as obtained `stage_id`s). A staged batch of tasks waits for all its dependencies to be in the `done` (or `error`) state(s), and is then added to the task queue.

```cpp
#include <array>
#include <chrono>
#include <random>
#include <dumb_tasks/task_queue.hpp>
#include <dumb_tasks/scheduler.hpp>

using namespace std::chrono;

int main() {
  static std::default_random_engine engine(std::random_device{}());
  static std::uniform_int_distribution<> dist(0, 30);
  auto const foo = [](std::size_t x) {
    std::this_thread::sleep_for(milliseconds(dist(engine)));
    std::string str("x = ");
    str += std::to_string(x);
    str += "\n";
    std::cout << str;
  };
  {
    // task queue
    dts::task_queue queue;
    std::array<dts::task_id, count> ids;
    for (std::size_t i = 0; i < 16; ++i) {
      ids[i] = queue.enqueue([i, &foo]() { foo(i); });
    }
    queue.wait_tasks(ids);
  }
  {
    // task scheduler
    dts::scheduler scheduler;
    std::array<dts::scheduler::stage_id, 4> ids;
    for (std::size_t i = 0; i < 4; ++i) {
      dts::scheduler::stage_t stage;
      for (std::size_t j = 0; j < 4; ++j) {
        stage.tasks.push_back([i, j, &foo]() { foo(5 * i + j); });
      }
      if (i > 0) {
        stage.deps = {ids[i - 1]};
      }
      ids[i] = scheduler.stage(stage);
    }
    scheduler.wait(ids.back());
  }
}
```

### Contributing

Pull/merge requests are welcome.
