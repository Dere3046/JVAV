#ifndef TEST_UTILS_HPP
#define TEST_UTILS_HPP
#include <cstdio>
#include <cstdlib>
#include <string>

inline int g_tests_passed = 0;
inline int g_tests_failed = 0;

inline void test_header(const char *name) {
    printf("[ RUN      ] %s\n", name);
}

inline void test_passed(const char *name) {
    printf("[       OK ] %s\n", name);
    g_tests_passed++;
}

inline void test_failed(const char *name, const char *msg) {
    printf("[  FAILED  ] %s: %s\n", name, msg);
    g_tests_failed++;
}

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { test_failed(__FUNCTION__, msg); return 1; } } while(0)

#define TEST_ASSERT_EQ(actual, expected, msg) \
    do { if ((actual) != (expected)) { \
        test_failed(__FUNCTION__, (std::string(msg) + " expected=" + std::to_string(expected) + " actual=" + std::to_string(actual)).c_str()); \
        return 1; \
    } } while(0)

#endif
