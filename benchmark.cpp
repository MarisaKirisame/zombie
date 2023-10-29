#define NDEBUG
/*
#include <iostream>
#include <fstream>
#include "zombie/heap/heap.hpp"
#include "nlohmann/json.hpp"
#include <chrono>
#include <valgrind/callgrind.h>

using json = nlohmann::json;

struct EmptyNotifyHeapIndexChanged {
  void operator()(const int&, const size_t&) {
  }
};

using KineticHanger = HeapImpls::KineticMinHeap<int, true, EmptyNotifyHeapIndexChanged>;

void noria() {
  std::ifstream f("../zombie.log");
  std::string line;
  std::vector<std::pair<int, int>> vec;
  while (std::getline(f, line)) {
    auto j = json::parse(line);
    vec.push_back(std::make_pair(static_cast<int>(j["mem_usage"]), static_cast<int>(j["t"])));
  }
  std::cout << "loading complete..." << std::endl;
  CALLGRIND_START_INSTRUMENTATION;
  CALLGRIND_TOGGLE_COLLECT;
  std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
  KineticHanger kh(0);
  for (size_t i = 0; i < vec.size(); ++i) {
    kh.push(0, AffFunction(-10000 / vec[i].first, -vec[i].second));
    if (i % 10000 == 0) {
      kh.debug();
    }
  }
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::cout << "time Spent = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " milliseconds" << std::endl;
  CALLGRIND_TOGGLE_COLLECT;
  CALLGRIND_STOP_INSTRUMENTATION;
}

std::vector<std::string> dtr_get_lines() {
  std::string line;
  std::ifstream f("../dtr.log");
  std::vector<std::string> lines;
  while (std::getline(f, line)) {
    lines.push_back(line);
  }
  return lines;
}

void dtr() {
  std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
  KineticHanger kh(0);
  size_t i = 0;
  size_t j = 0;
  auto lines = dtr_get_lines();
  std::optional<int64_t> start_time;
  CALLGRIND_START_INSTRUMENTATION;
  CALLGRIND_TOGGLE_COLLECT;
  for (const auto& line: lines) {
   std::istringstream lbuf(line);
    std::string head;
    lbuf >> head;
    if (head == "push") {
      size_t ptr;
      int64_t slope;
      int64_t shift;
      lbuf >> ptr >> slope >> shift;
      if (!start_time) {
        start_time = -shift;
      }
      shift += start_time.value();
      kh.push(0, AffFunction(slope, shift));
      if (j % 100000 == 0) {
        std::cout << "slope: "<< slope << " shift: " << shift << " slope * shift: " << slope * shift << std::endl;
      }
      ++j;
    } else if (head == "remove") {
      
    } else if (head == "advance") {
      int64_t time;
      lbuf >> time;
      time -= start_time.value();
      kh.advance_to(time);
    } else if (head == "pope") {
      kh.pop();
    } else if (head == "repush") {
      
    } else {
      std::cout << head << std::endl;
      throw;
    }
    if (i % (1000 * 1000) == 0) {
      kh.debug();
      std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
      std::cout << "time Spent = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " milliseconds" << std::endl;
    }
    ++i;
  }
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::cout << "time Spent = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " milliseconds" << std::endl;
  CALLGRIND_TOGGLE_COLLECT;
  CALLGRIND_STOP_INSTRUMENTATION;
}

int main() {
  dtr();
}
*/