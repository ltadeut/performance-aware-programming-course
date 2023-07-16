struct profiler_section {
  const char *Name;
  u64 ElapsedInclusive;
  u64 ElapsedExclusive;
  u64 Hits;
};

class profiler_trace {
private:
  u64 mStartCounter;
  u64 mPreviousElapsedInclusive;
  const char* mSectionName;
  u32 mSectionIndex;
  u32 mParentSectionIndex;

public:
  profiler_trace(const char *SectionName, u32 SectionIndex);
  ~profiler_trace();
};

struct profiler {
  profiler_section Sections[4096];
  u32 ActiveSectionIndex;
  u32 SectionsCount;
  u64 StartCounter;
  u64 EndCounter;
  u64 CPUFrequency;
};

static profiler GlobalProfiler;

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

profiler_trace::profiler_trace(const char *SectionName, u32 SectionIndex) {
  mSectionName = SectionName;
  mSectionIndex = SectionIndex;
  mParentSectionIndex = GlobalProfiler.ActiveSectionIndex;
  mPreviousElapsedInclusive = GlobalProfiler.Sections[mSectionIndex].ElapsedInclusive;
  
  GlobalProfiler.ActiveSectionIndex = SectionIndex;

  mStartCounter = ReadCPUTimer();
}

profiler_trace::~profiler_trace() {
  u64 EndCounter = ReadCPUTimer();
  u64 Elapsed = EndCounter - mStartCounter;
  GlobalProfiler.ActiveSectionIndex = mParentSectionIndex;

  profiler_section* Parent = GlobalProfiler.Sections + mParentSectionIndex;
  profiler_section* Section = GlobalProfiler.Sections + mSectionIndex;

  // Pop the active section
  Parent->ElapsedExclusive -= Elapsed;
  Section->ElapsedExclusive += Elapsed;
  Section->ElapsedInclusive = mPreviousElapsedInclusive + Elapsed;
  Section->Name = mSectionName;
  Section->Hits++;
}

#define NameConcat2(A, B) A##B
#define NameConcat(A, B) NameConcat2(A, B)
#define TimeBlock(NAME) profiler_trace NameConcat(Trace, __LINE__)(NAME, __COUNTER__ + 1)
#define TimeFunction TimeBlock(__func__)
#define ArrayCount(A) (sizeof(A) / sizeof(A[0]))

void BeginProfile() {
  GlobalProfiler.CPUFrequency = EstimateCPUFrequency();
  GlobalProfiler.StartCounter = ReadCPUTimer();
}

void EndProfileAndPrint() {
  GlobalProfiler.EndCounter = ReadCPUTimer();

  u64 TotalElapsed = (GlobalProfiler.EndCounter - GlobalProfiler.StartCounter);

  printf("== Profiling results (Total ticks: %llu)\n", TotalElapsed);

  for (u32 Index = 1; Index < ArrayCount(GlobalProfiler.Sections); Index++) {
    profiler_section *Section = &GlobalProfiler.Sections[Index];

    if (Section->Name) {
	    printf("\t%s[%llu]: %llu, %llu(%.2f%%, %.2f%%)\n", Section->Name,
		   Section->Hits, Section->ElapsedInclusive, Section->ElapsedExclusive,
		   100.0 * Section->ElapsedInclusive / TotalElapsed, 100.0 * Section->ElapsedExclusive / TotalElapsed);
    }
  }
}
