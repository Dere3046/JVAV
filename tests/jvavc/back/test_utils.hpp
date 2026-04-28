#ifndef TEST_UTILS_HPP
#define TEST_UTILS_HPP
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

inline int system_exit_code(int status) {
#ifdef _WIN32
    return status;
#else
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
#endif
}

inline bool read_file(const std::string &path, std::string &out) {
    std::ifstream f(path);
    if (!f) return false;
    out = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

inline bool write_file(const std::string &path, const std::string &content) {
    std::ofstream f(path);
    if (!f) return false;
    f << content;
    return true;
}

inline int g_tests_passed = 0;
inline int g_tests_failed = 0;
inline bool g_update_snapshots = false;

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

inline std::string get_snapshot_dir() {
#ifdef TEST_SOURCE_DIR
    return std::string(TEST_SOURCE_DIR) + "/tests/snapshots";
#else
    return "tests/snapshots";
#endif
}

inline bool check_snapshot(const std::string& actual, const std::string& snapshot_name,
                           const std::string& snapshot_dir = "") {
    std::string dir = snapshot_dir.empty() ? get_snapshot_dir() : snapshot_dir;
    std::string path = dir + "/" + snapshot_name + ".snap";
    std::string expected;
    bool has_expected = read_file(path, expected);
    if (g_update_snapshots || !has_expected) {
        if (!write_file(path, actual)) {
            printf("  FAIL: cannot write snapshot %s\n", path.c_str());
            return false;
        }
        if (g_update_snapshots) {
            printf("  UPDATED snapshot %s\n", path.c_str());
        } else {
            printf("  CREATED snapshot %s\n", path.c_str());
        }
        return true;
    }
    if (actual != expected) {
        printf("  FAIL: snapshot mismatch for %s\n", snapshot_name.c_str());
        // Print a simple line-by-line diff
        size_t a_pos = 0, e_pos = 0;
        int line_num = 1;
        while (a_pos < actual.size() || e_pos < expected.size()) {
            size_t a_end = actual.find('\n', a_pos);
            if (a_end == std::string::npos) a_end = actual.size();
            size_t e_end = expected.find('\n', e_pos);
            if (e_end == std::string::npos) e_end = expected.size();
            std::string a_line = actual.substr(a_pos, a_end - a_pos);
            std::string e_line = expected.substr(e_pos, e_end - e_pos);
            if (a_line != e_line) {
                printf("    - %s\n", e_line.c_str());
                printf("    + %s\n", a_line.c_str());
            }
            a_pos = a_end + 1;
            e_pos = e_end + 1;
            line_num++;
            if (line_num > 30) { printf("    ... (diff truncated)\n"); break; }
        }
        return false;
    }
    return true;
}

#define TEST_ASSERT_SNAPSHOT_EQ(actual, name) \
    do { if (!check_snapshot((actual), (name))) { test_failed(__FUNCTION__, (std::string("snapshot mismatch: ") + (name)).c_str()); return 1; } } while(0)

#endif
