#if __arm__ || __aarch64__
static u64 ReadCPUTimer() {
  u64 Result;

  asm volatile("isb;\n\tmrs %0, cntvct_el0" : "=r"(Result)::"memory");

  return Result;
}

static u64 EstimateCPUFrequency() {
  u64 Result;

  asm volatile("mrs %0, cntfrq_el0" : "=r"(Result)::"memory");

  return Result;
}

#elif _WIN64

#include <intrin.h>
#include <windows.h>

static u64 GetOSTimerFreq(void) {
  LARGE_INTEGER Freq;
  QueryPerformanceFrequency(&Freq);
  return Freq.QuadPart;
}

static u64 ReadOSTimer(void) {
  LARGE_INTEGER Value;
  QueryPerformanceCounter(&Value);
  return Value.QuadPart;
}

static u64 ReadCPUTimer() { return __rdtsc(); }

static u64 EstimateCPUFrequency(void) {
  u64 MillisecondsToWait = 100;
  u64 OSFreq = GetOSTimerFreq();

  u64 CPUStart = ReadCPUTimer();
  u64 OSStart = ReadOSTimer();
  u64 OSEnd = 0;
  u64 OSElapsed = 0;
  u64 OSWaitTime = OSFreq * MillisecondsToWait / 1000;
  while (OSElapsed < OSWaitTime) {
    OSEnd = ReadOSTimer();
    OSElapsed = OSEnd - OSStart;
  }

  u64 CPUEnd = ReadCPUTimer();
  u64 CPUElapsed = CPUEnd - CPUStart;

  u64 CPUFreq = 0;
  if (OSElapsed) {
    CPUFreq = OSFreq * CPUElapsed / OSElapsed;
  }

  return CPUFreq;
}
#endif
