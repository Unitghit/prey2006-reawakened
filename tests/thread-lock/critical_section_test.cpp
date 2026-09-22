#include <stdexcept>
#include <cstdio>
static const int CRITICAL_SECTION_ZERO = 0;
static int depth[5] = {};
void Sys_EnterCriticalSection(int index) { ++depth[index]; }
void Sys_LeaveCriticalSection(int index) { --depth[index]; }
#include "../../neo/sys/ScopedCriticalSection.h"
static void throwFromNestedSoundUpdate() {
    idScopedCriticalSection nested;
    throw std::runtime_error("invalid portal area");
}
static void soundStart() {
    idScopedCriticalSection outer;
    throwFromNestedSoundUpdate();
}
static void earlyReturn() { idScopedCriticalSection lock(2); return; }
int main() {
    try { soundStart(); } catch (const std::runtime_error&) {}
    earlyReturn();
    for (int count : depth) { if (count != 0) return 1; }
    { idScopedCriticalSection lock; if (depth[0] != 1) return 2; }
    if (depth[0] != 0) return 3;
    std::puts("PASS: nested exception, early return, and normal completion release locks");
}
