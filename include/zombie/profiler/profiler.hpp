#pragma once

#include "../common.hpp"

#include <chrono>
#include <map>
#include <iostream>
#include <string>
#include <stack>

// Profiler only works for single-thread program now
struct Profiler {
    std::map<std::string, ns> counts;

    ~Profiler() {
        std::cout << "Profiler Result: " << std::endl;
        for (auto [s, t] : counts) {
            std::cout << s << ": " << t << std::endl;
        }
    }

    static void count(std::string s, ns t) {
        static Profiler pf;
        pf.counts[s] += t;
    }
};

struct TimeCounter {
    using time_t = decltype(std::chrono::steady_clock::now());

    static std::stack<std::string> stack;
    static time_t last_time;

    std::string name;

    TimeCounter(std::string name) : name(name) {
        auto t = std::chrono::steady_clock::now();

        if (!stack.empty()) {
            Profiler::count(stack.top(), ns(t - last_time));
        }

        stack.push(name);
        last_time = std::chrono::steady_clock::now();
        // get now time again so that we can ignore the cost of Profiler
    }

    ~TimeCounter() {
        auto t = std::chrono::steady_clock::now();

        assert(stack.top() == this->name);

        Profiler::count(stack.top(), ns(t - last_time));
        
        stack.pop();
        last_time = std::chrono::steady_clock::now();
    }
};