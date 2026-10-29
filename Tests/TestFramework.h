#pragma once

// A deliberately tiny, dependency-free test harness.
//   ZS_TEST(name) { ... ZS_CHECK(cond); ZS_CHECK_NEAR(a, b, tol); ... }
// Run the binary with an optional substring filter: ./ZSynthDspTests filter

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace zstest
{

struct TestCase
{
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> tests;
    return tests;
}

inline int& failures()
{
    static int f = 0;
    return f;
}

struct Registrar
{
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({ name, std::move(fn) }); }
};

inline void report(bool ok, const char* expr, const char* file, int line, const std::string& detail = {})
{
    if (! ok)
    {
        ++failures();
        std::printf("    FAILED: %s  (%s:%d) %s\n", expr, file, line, detail.c_str());
    }
}

inline void note(const char* fmt, double value)
{
    std::printf("    %s", "");
    std::printf(fmt, value);
    std::printf("\n");
}

inline int runAll(int argc, char** argv)
{
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failedTests = 0;
    for (auto& t : registry())
    {
        if (filter != nullptr && std::strstr(t.name, filter) == nullptr)
            continue;
        const int before = failures();
        std::printf("[ RUN  ] %s\n", t.name);
        std::fflush(stdout);
        t.fn();
        const bool ok = failures() == before;
        std::printf("[ %s ] %s\n", ok ? " OK " : "FAIL", t.name);
        ++run;
        failedTests += ok ? 0 : 1;
    }
    std::printf("\n%d tests run, %d failed, %d failed checks\n", run, failedTests, failures());
    return failedTests == 0 ? 0 : 1;
}

} // namespace zstest

#define ZS_CONCAT2(a, b) a##b
#define ZS_CONCAT(a, b) ZS_CONCAT2(a, b)
#define ZS_TEST(name) \
    static void ZS_CONCAT(test_, name)(); \
    static zstest::Registrar ZS_CONCAT(reg_, name)(#name, ZS_CONCAT(test_, name)); \
    static void ZS_CONCAT(test_, name)()

#define ZS_CHECK(cond) zstest::report((cond), #cond, __FILE__, __LINE__)
#define ZS_CHECK_NEAR(a, b, tol) \
    do { const double zs_a = (a), zs_b = (b); \
         zstest::report(std::abs(zs_a - zs_b) <= (tol), #a " ~= " #b, __FILE__, __LINE__, \
                        "(" + std::to_string(zs_a) + " vs " + std::to_string(zs_b) + ")"); } while (0)
#define ZS_CHECK_LT(a, b) \
    do { const double zs_a = (a), zs_b = (b); \
         zstest::report(zs_a < zs_b, #a " < " #b, __FILE__, __LINE__, \
                        "(" + std::to_string(zs_a) + " vs " + std::to_string(zs_b) + ")"); } while (0)
#define ZS_CHECK_GT(a, b) \
    do { const double zs_a = (a), zs_b = (b); \
         zstest::report(zs_a > zs_b, #a " > " #b, __FILE__, __LINE__, \
                        "(" + std::to_string(zs_a) + " vs " + std::to_string(zs_b) + ")"); } while (0)
