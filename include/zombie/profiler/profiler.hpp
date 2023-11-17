#pragma once

#include "../common.hpp"

#include <chrono>
#include <map>
#include <iostream>
#include <string>
#include <stack>

// Profiler only works for single-thread program now
struct Profiler {
    using time_t = decltype(std::chrono::steady_clock::now());

    std::map<std::string, ns> counts;
    std::stack<std::string> stack;
    time_t last_time;

    ~Profiler() {
        std::cout << "Profiler Result: " << std::endl;
        for (auto [s, t] : counts) {
            std::cout << s << ": " << t.count() << std::endl;
        }
    }

    static Profiler& singleton() {
        static Profiler pf;
        return pf;
    }

    static void count(std::string s, ns t) {
        auto& pf = singleton();
        pf.counts[s] += t;
    }
};

struct TimeCounter {
    std::string name;

    TimeCounter(std::string name) : name(name) {
        auto t = std::chrono::steady_clock::now();
        auto& pf = Profiler::singleton();

        if (!pf.stack.empty()) {
            Profiler::count(pf.stack.top(), ns(t - pf.last_time));
        }

        pf.stack.push(name);
        pf.last_time = std::chrono::steady_clock::now();
        // get now time again so that we can ignore the cost of Profiler
    }

    ~TimeCounter() {
        auto t = std::chrono::steady_clock::now();
        auto& pf = Profiler::singleton();

        if (pf.stack.empty() || pf.stack.top() != this->name) {
            if (pf.stack.empty()) {
                puts("!!!!!!!");
            } else {
                std::cout << pf.stack.top() << " " << this->name << std::endl;
            }
        }
        // assert(pf.stack.top() == this->name);

        Profiler::count(pf.stack.top(), ns(t - pf.last_time));
        
        pf.stack.pop();
        pf.last_time = std::chrono::steady_clock::now();
    }
};