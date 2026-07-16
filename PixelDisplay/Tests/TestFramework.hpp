// PixelDisplay Pro — Tests/TestFramework.hpp
//
// A tiny, dependency-free test harness. We deliberately avoid pulling an
// external framework so the engine tests build reproducibly with no network
// access (DESIGN.md §16). Register tests with PD_TEST; run with pd_test::run().
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace pd_test {

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> fn) {
        registry().push_back({std::move(name), std::move(fn)});
    }
};

// Thrown by failing checks to abort the current case.
struct Failure {
    std::string message;
};

inline void checkTrue(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        char buf[512];
        std::snprintf(buf, sizeof buf, "%s:%d: expected true: %s", file, line, expr);
        throw Failure{buf};
    }
}

template <typename A, typename B>
void checkEq(const A& a, const B& b, const char* ea, const char* eb, const char* file, int line) {
    if (!(a == b)) {
        char buf[512];
        std::snprintf(buf, sizeof buf, "%s:%d: expected equal: %s == %s", file, line, ea, eb);
        throw Failure{buf};
    }
}

inline void checkNear(double a, double b, double tol, const char* file, int line) {
    if (std::fabs(a - b) > tol) {
        char buf[512];
        std::snprintf(buf, sizeof buf, "%s:%d: |%.9g - %.9g| > %.9g", file, line, a, b, tol);
        throw Failure{buf};
    }
}

inline int run() {
    int failed = 0;
    for (const auto& c : registry()) {
        try {
            c.fn();
            std::printf("[ PASS ] %s\n", c.name.c_str());
        } catch (const Failure& f) {
            std::printf("[ FAIL ] %s\n         %s\n", c.name.c_str(), f.message.c_str());
            ++failed;
        } catch (const std::exception& e) {
            std::printf("[ FAIL ] %s\n         unexpected exception: %s\n", c.name.c_str(), e.what());
            ++failed;
        }
    }
    std::printf("\n%zu tests, %d failed\n", pd_test::registry().size(), failed);
    return failed == 0 ? 0 : 1;
}

}  // namespace pd_test

#define PD_CONCAT_(a, b) a##b
#define PD_CONCAT(a, b) PD_CONCAT_(a, b)
#define PD_TEST(name)                                                        \
    static void PD_CONCAT(pd_test_fn_, __LINE__)();                          \
    static ::pd_test::Registrar PD_CONCAT(pd_test_reg_, __LINE__){           \
        name, &PD_CONCAT(pd_test_fn_, __LINE__)};                            \
    static void PD_CONCAT(pd_test_fn_, __LINE__)()

#define PD_CHECK(cond) ::pd_test::checkTrue((cond), #cond, __FILE__, __LINE__)
#define PD_CHECK_EQ(a, b) ::pd_test::checkEq((a), (b), #a, #b, __FILE__, __LINE__)
#define PD_CHECK_NEAR(a, b, tol) ::pd_test::checkNear((a), (b), (tol), __FILE__, __LINE__)
