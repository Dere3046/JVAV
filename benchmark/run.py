#!/usr/bin/env python3
"""JVAV Performance Benchmark Suite

Runs a set of compute/memory benchmarks through the JVAV toolchain
(jvlc -> jvavc -> jvm) and compares against native C equivalents.
"""

import os
import sys
import subprocess
import tempfile
import time

# ------------------------------------------------------------------
# Config
# ------------------------------------------------------------------
BUILD_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build")
BIN_DIR = os.path.join(BUILD_DIR, "bin")
JVLC = os.path.join(BIN_DIR, "jvlc.exe")
JVC = os.path.join(BIN_DIR, "jvavc.exe")
JVM = os.path.join(BIN_DIR, "jvm.exe")
CC = "gcc"

# ------------------------------------------------------------------
# Benchmark definitions
# ------------------------------------------------------------------

BENCHMARKS = []

# 1. Integer accumulation
BENCHMARKS.append({
    "name": "integer_accumulation",
    "desc": "Sum integers from 0 to 999,999",
    "jvl": '''func main(): int {
    var sum: int = 0;
    var i: int = 0;
    while (i < 1000000) {
        sum = sum + i;
        i = i + 1;
    }
    putint(sum);
    return 0;
}''',
    "c": '''#include <stdio.h>
int main() {
    long long sum = 0;
    for (int i = 0; i < 1000000; i++) sum += i;
    printf("%lld", sum);
    return 0;
}''',
})

# 2. Fibonacci recursive
BENCHMARKS.append({
    "name": "fib_recursive",
    "desc": "Fibonacci(20) recursive",
    "jvl": '''func fib(n: int): int {
    if (n <= 1) { return n; }
    var a: int = fib(n - 1);
    var b: int = fib(n - 2);
    return a + b;
}
func main(): int {
    putint(fib(20));
    return 0;
}''',
    "c": '''#include <stdio.h>
int fib(int n) { return n <= 1 ? n : fib(n-1) + fib(n-2); }
int main() { printf("%d", fib(20)); return 0; }''',
})

# 3. Fibonacci iterative
BENCHMARKS.append({
    "name": "fib_iterative",
    "desc": "Fibonacci(50) iterative",
    "jvl": '''func main(): int {
    var n: int = 50;
    var a: int = 0;
    var b: int = 1;
    var i: int = 2;
    while (i <= n) {
        var c: int = a + b;
        a = b;
        b = c;
        i = i + 1;
    }
    putint(b);
    return 0;
}''',
    "c": '''#include <stdio.h>
int main() {
    long long a = 0, b = 1;
    for (int i = 2; i <= 50; i++) { long long c = a + b; a = b; b = c; }
    printf("%lld", b);
    return 0;
}''',
})

# 4. Heap pressure
BENCHMARKS.append({
    "name": "heap_pressure",
    "desc": "5000 allocations of 100 words each",
    "jvl": '''func main(): int {
    var i: int = 0;
    while (i < 5000) {
        var p: ptr<int> = alloc(100);
        p[0] = i;
        p[99] = i;
        free(p);
        i = i + 1;
    }
    putint(i);
    return 0;
}''',
    "c": '''#include <stdio.h>
#include <stdlib.h>
int main() {
    for (int i = 0; i < 5000; i++) {
        int* p = (int*)malloc(100 * sizeof(int));
        p[0] = i; p[99] = i;
        free(p);
    }
    printf("%d", 5000);
    return 0;
}''',
})

# 5. Bubble sort
BENCHMARKS.append({
    "name": "bubble_sort",
    "desc": "Bubble sort 200 elements",
    "jvl": '''func main(): int {
    var n: int = 200;
    var arr: ptr<int> = alloc(n);
    var i: int = 0;
    while (i < n) {
        arr[i] = n - i;
        i = i + 1;
    }
    i = 0;
    while (i < n) {
        var j: int = 0;
        while (j < n - 1 - i) {
            if (arr[j] > arr[j + 1]) {
                var tmp: int = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = tmp;
            }
            j = j + 1;
        }
        i = i + 1;
    }
    putint(arr[0]);
    putint(arr[n - 1]);
    free(arr);
    return 0;
}''',
    "c": '''#include <stdio.h>
#include <stdlib.h>
int main() {
    int n = 200;
    int* arr = (int*)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) arr[i] = n - i;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n - 1 - i; j++)
            if (arr[j] > arr[j+1]) { int t = arr[j]; arr[j] = arr[j+1]; arr[j+1] = t; }
    printf("%d%d", arr[0], arr[n-1]);
    free(arr);
    return 0;
}''',
})

# 6. Prime sieve
BENCHMARKS.append({
    "name": "prime_sieve",
    "desc": "Sieve of Eratosthenes up to 3000",
    "jvl": '''func main(): int {
    var n: int = 3000;
    var sieve: ptr<int> = alloc(n);
    var i: int = 0;
    while (i < n) {
        sieve[i] = 1;
        i = i + 1;
    }
    sieve[0] = 0;
    sieve[1] = 0;
    i = 2;
    while (i * i < n) {
        if (sieve[i] == 1) {
            var j: int = i * i;
            while (j < n) {
                sieve[j] = 0;
                j = j + i;
            }
        }
        i = i + 1;
    }
    var count: int = 0;
    i = 0;
    while (i < n) {
        if (sieve[i] == 1) { count = count + 1; }
        i = i + 1;
    }
    putint(count);
    free(sieve);
    return 0;
}''',
    "c": '''#include <stdio.h>
#include <stdlib.h>
int main() {
    int n = 3000;
    int* sieve = (int*)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) sieve[i] = 1;
    sieve[0] = sieve[1] = 0;
    for (int i = 2; i * i < n; i++)
        if (sieve[i])
            for (int j = i * i; j < n; j += i) sieve[j] = 0;
    int count = 0;
    for (int i = 0; i < n; i++) if (sieve[i]) count++;
    printf("%d", count);
    free(sieve);
    return 0;
}''',
})

# ------------------------------------------------------------------
# Helpers
# ------------------------------------------------------------------

def run_cmd(cmd, cwd=None):
    """Run a command, return (returncode, stdout, stderr, elapsed_ms)"""
    start = time.perf_counter()
    try:
        proc = subprocess.run(cmd, shell=True, cwd=cwd, capture_output=True, text=True, timeout=300)
    except subprocess.TimeoutExpired:
        return -1, "", "TIMEOUT", 300000
    elapsed = (time.perf_counter() - start) * 1000
    return proc.returncode, proc.stdout, proc.stderr, elapsed

def check_tools():
    missing = []
    for path, name in [(JVLC, "jvlc"), (JVC, "jvavc"), (JVM, "jvm")]:
        if not os.path.exists(path):
            missing.append(name)
    if missing:
        print(f"ERROR: Missing tools: {missing}")
        print(f"Expected build directory: {BUILD_DIR}")
        print("Please build the project first: mkdir build && cd build && cmake .. && make")
        sys.exit(1)

def run_jvav(src_jvl, tmpdir):
    """Compile JVL -> JVAV -> BIN and run. Returns (ok, output, elapsed_ms)"""
    jvl_path = os.path.join(tmpdir, "test.jvl")
    jvav_path = os.path.join(tmpdir, "test.jvav")
    bin_path = os.path.join(tmpdir, "test.bin")
    with open(jvl_path, "w") as f:
        f.write(src_jvl)

    # jvlc
    rc, out, err, t1 = run_cmd(f'"{JVLC}" "{jvl_path}" "{jvav_path}"')
    if rc != 0:
        return False, f"jvlc failed: {err}", t1

    # jvavc
    rc, out, err, t2 = run_cmd(f'"{JVC}" "{jvav_path}" "{bin_path}"')
    if rc != 0:
        return False, f"jvavc failed: {err}", t2

    # jvm
    rc, out, err, t3 = run_cmd(f'"{JVM}" "{bin_path}"')
    if rc != 0:
        return False, f"jvm failed: {err}", t3

    return True, out.strip(), t3

def run_c(src_c, tmpdir):
    """Compile and run C reference. Returns (ok, output, elapsed_ms)"""
    c_path = os.path.join(tmpdir, "ref.c")
    exe_path = os.path.join(tmpdir, "ref.exe")
    with open(c_path, "w") as f:
        f.write(src_c)

    rc, out, err, t1 = run_cmd(f'gcc -O2 -o "{exe_path}" "{c_path}"')
    if rc != 0:
        return False, f"gcc failed: {err}", t1

    rc, out, err, t2 = run_cmd(f'"{exe_path}"')
    if rc != 0:
        return False, f"native run failed: {err}", t2

    return True, out.strip(), t2

# ------------------------------------------------------------------
# Main
# ------------------------------------------------------------------

def main():
    check_tools()

    print("=" * 72)
    print("JVAV Performance Benchmark Suite")
    print("=" * 72)
    print()

    results = []

    for bench in BENCHMARKS:
        name = bench["name"]
        desc = bench["desc"]
        print(f"[ {name} ] {desc}")
        print("-" * 72)

        with tempfile.TemporaryDirectory() as tmpdir:
            # JVAV
            ok_j, out_j, t_j = run_jvav(bench["jvl"], tmpdir)
            status_j = "OK" if ok_j else "FAIL"

            # Native C
            ok_c, out_c, t_c = run_c(bench["c"], tmpdir)
            status_c = "OK" if ok_c else "FAIL"

            # Verify output match
            match = (ok_j and ok_c and out_j == out_c)

            if ok_j and ok_c:
                ratio = t_j / t_c if t_c > 0 else float('inf')
                print(f"  JVAV output : {out_j:<20}  time: {t_j:>10.2f} ms  [{status_j}]")
                print(f"  Native output: {out_c:<20}  time: {t_c:>10.2f} ms  [{status_c}]")
                print(f"  Ratio        : JVAV is {ratio:.1f}x slower than native C")
                print(f"  Match        : {'PASS' if match else 'MISMATCH'}")
                results.append({"name": name, "jvav_ms": t_j, "native_ms": t_c, "ratio": ratio, "match": match})
            else:
                print(f"  JVAV  : {out_j}  [{status_j}]")
                print(f"  Native: {out_c}  [{status_c}]")
                results.append({"name": name, "jvav_ms": t_j if ok_j else None, "native_ms": t_c if ok_c else None, "ratio": None, "match": False})
        print()

    # Summary
    print("=" * 72)
    print("SUMMARY")
    print("=" * 72)
    print(f"{'Benchmark':<25} {'JVAV (ms)':>12} {'Native (ms)':>12} {'Ratio':>10} {'Match':>6}")
    print("-" * 72)
    for r in results:
        jv = f"{r['jvav_ms']:.2f}" if r['jvav_ms'] is not None else "N/A"
        nc = f"{r['native_ms']:.2f}" if r['native_ms'] is not None else "N/A"
        rt = f"{r['ratio']:.1f}x" if r['ratio'] is not None else "N/A"
        mt = "PASS" if r['match'] else "FAIL"
        print(f"{r['name']:<25} {jv:>12} {nc:>12} {rt:>10} {mt:>6}")

    all_pass = all(r["match"] for r in results)
    print("-" * 72)
    print(f"Overall: {'ALL PASS' if all_pass else 'SOME FAILED'}")
    return 0 if all_pass else 1

if __name__ == "__main__":
    sys.exit(main())
