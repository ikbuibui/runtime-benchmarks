// The skynet benchmark as described here:
// https://github.com/atemerev/skynet

// Adapted from https://github.com/tzcnt/tmc-examples/blob/main/examples/skynet/main.cpp
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
#include "tmc/sync.hpp"
#include "tmc/task.hpp"
#include "tmc/spawn_task_many.hpp"
#include "tmc/utils.hpp"

#include <chrono>
#include <cinttypes>
#include <cstdio>

static size_t thread_count = std::thread::hardware_concurrency()/2;
static const size_t iter_count = 1;

template <size_t DepthMax>
tmc::task<size_t> skynet_one(size_t BaseNum, size_t Depth) {
  if (Depth == DepthMax) {
    co_return BaseNum;
  }
  size_t depthOffset = 1;
  for (size_t i = 0; i < DepthMax - Depth - 1; ++i) {
    depthOffset *= 10;
  }

  /// Simplest way to spawn subtasks
  // std::array<tmc::task<size_t>, 10> children;
  // for (size_t idx = 0; idx < 10; ++idx) {
  //   children[idx] = skynet_one<DepthMax>(BaseNum + depthOffset * idx,
  //   Depth + 1);
  // }
  // std::array<size_t, 10> results = co_await
  // tmc::spawn_many<10>(children.data());

  /// Concise and slightly faster way to run subtasks
  std::array<size_t, 10> results =
    co_await tmc::spawn_many<10>(tmc::iter_adapter(
      0ULL,
      [=](size_t idx) -> tmc::task<size_t> {
        return skynet_one<DepthMax>(BaseNum + depthOffset * idx, Depth + 1);
      }
    ));

  size_t count = 0;
  for (size_t idx = 0; idx < 10; ++idx) {
    count += results[idx];
  }
  co_return count;
}
template <size_t DepthMax> tmc::task<void> skynet() {
  size_t count = co_await skynet_one<DepthMax>(0, 0);
  if (count != 499999500000) {
    std::printf("%" PRIu64 "\n", count);
  }
}

template <size_t Depth = 6> tmc::task<void> loop_skynet() {
  for (size_t j = 0; j < 5; ++j) {
    auto startTime = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iter_count; ++i) {
      co_await skynet<Depth>();
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto execDur = endTime - startTime;
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(
      endTime - startTime
    );
    std::printf(
      "%" PRIu64 " iterations in %" PRIu64 " us\n",
      iter_count, totalTimeUs.count()
    );
  }
}

int main() {
  tmc::cpu_executor().set_thread_count(thread_count).init();
  std::printf("Using %" PRIu64 " threads.\n", thread_count);
  return tmc::async_main([]() -> tmc::task<int> {
    co_await loop_skynet<6>();
    co_return 0;
  }());
}