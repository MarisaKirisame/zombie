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
            std::cout << s << ": " << t.count() << std::endl;
        }
    }

    static void count(std::string s, ns t) {
        static Profiler pf;
        pf.counts[s] += t;
    }
};

struct TimeCounter {
    using time_t = decltype(std::chrono::steady_clock::now());

    static std::stack<std::string> stk;

    std::string name;

    static time_t& get_last_time() {
        static time_t last_time;
        return last_time;
    }

    static std::stack<std::string>& get_stack() {
        static std::stack<std::string> stk;
        return stk;
    }

    TimeCounter(std::string name) : name(name) {
        auto t = std::chrono::steady_clock::now();

        if (!stk.empty()) {
            Profiler::count(stk.top(), ns(t - this->get_last_time()));
        }

        this->get_stack().push(name);
        this->get_last_time() = std::chrono::steady_clock::now();
        // get now time again so that we can ignore the cost of Profiler
    }

    ~TimeCounter() {
        auto t = std::chrono::steady_clock::now();

        assert(stk.top() == this->name);

        Profiler::count(stk.top(), ns(t - this->get_last_time()));
        
        this->get_stack().pop();
        this->get_last_time() = std::chrono::steady_clock::now();
    }
};