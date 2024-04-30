// An implementation of the recursive fork fibonacci parallelism test.

// Adapted from https://github.com/tzcnt/tmc-examples/blob/main/examples/fib.cpp
// Original author: tzcnt
// Unlicense License
// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#define TMC_IMPL

#include "tmc/ex_cpu.hpp"
#include "tmc/spawn_task.hpp"
#include "tmc/spawn_task_many.hpp"
#include "tmc/utils.hpp"

#include <chrono>
#include <cinttypes>
#include <cstdio>

using namespace tmc;
static size_t thread_count = std::thread::hardware_concurrency()/2;
static const size_t iter_count = 1;

// Executes all tasks serially
static task<size_t> fib_serial(size_t n) {
  if (n < 2)
    co_return n;
  auto x = co_await fib_serial(n - 2);
  auto y = co_await fib_serial(n - 1);
  co_return x + y;
}

// Fork both tasks in parallel by submitting them in bulk to the executor.
static task<size_t> fib_bulk_array(size_t n) {
  if (n < 2)
    co_return n;

  std::array<task<size_t>, 2> tasks;
  tasks[0] = fib_bulk_array(n - 2);
  tasks[1] = fib_bulk_array(n - 1);
  auto results = co_await spawn_many<2>(tasks.data());
  co_return results[0] + results[1];
}

// Fork both tasks in parallel by submitting them in bulk to the executor.
static task<size_t> fib_bulk_iter(size_t n) {
  if (n < 2)
    co_return n;

  auto results = co_await spawn_many<2>(iter_adapter(n - 2, fib_bulk_iter));
  co_return results[0] + results[1];

}

// Spawn a hot task that runs in parallel with the current task, then continues the other leg serially.
static task<size_t> fib_hot(size_t n) {
  if (n < 2)
    co_return n;

  auto x_hot = spawn(fib_hot(n - 1)).run_early();
  auto y = co_await fib_hot(n - 2);
  auto x = co_await std::move(x_hot);
  co_return x + y;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    printf("Usage: fib <n-th fibonacci number requested>\n");
    exit(0);
  }
  size_t n = static_cast<size_t>(atoi(argv[1]));

  std::printf("threads: %" PRIu64 "\n", thread_count);
  tmc::cpu_executor().set_thread_count(thread_count).init();
  std::printf("results:\n");

  return tmc::async_main([](size_t N) -> tmc::task<int> {
    auto result = co_await fib_hot(30); // warmup

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iter_count; ++i) {
      auto result = co_await fib_hot(N);
      std::printf("  - %" PRIu64 "\n", result);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf("runs:\n");
    std::printf("  - iteration_count: %" PRIu64 "\n",iter_count);
    std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
    co_return 0;
  }(n));
}
