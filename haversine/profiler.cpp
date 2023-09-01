#ifndef PROFILER_ENABLED
#define PROFILER_ENABLED 0
#endif

#if PROFILER_ENABLED

struct profiler_section {
  const char *Name;
  u64 ElapsedInclusive;
  u64 ElapsedExclusive;
  u64 Hits;
  u64 ProcessedByteCount;
};

static profiler_section GlobalProfilerSections[4096];
static u32 GlobalActiveSectionIndex;

class profiler_trace {
private:
  u64 mStartCounter;
  u64 mPreviousElapsedInclusive;
  const char *mSectionName;
  u32 mSectionIndex;
  u32 mParentSectionIndex;

public:
  profiler_trace(const char *SectionName, u32 SectionIndex, u64 ByteCount);
  ~profiler_trace();
};

profiler_trace::profiler_trace(const char *SectionName, u32 SectionIndex,
                               u64 ByteCount) {
  mSectionName = SectionName;
  mSectionIndex = SectionIndex;
  mParentSectionIndex = GlobalActiveSectionIndex;
  mPreviousElapsedInclusive =
      GlobalProfilerSections[mSectionIndex].ElapsedInclusive;
  GlobalProfilerSections[mSectionIndex].ProcessedByteCount += ByteCount;
  GlobalActiveSectionIndex = SectionIndex;

  mStartCounter = ReadCPUTimer();
}

profiler_trace::~profiler_trace() {
  u64 EndCounter = ReadCPUTimer();
  u64 Elapsed = EndCounter - mStartCounter;
  GlobalActiveSectionIndex = mParentSectionIndex;

  profiler_section *Parent = GlobalProfilerSections + mParentSectionIndex;
  profiler_section *Section = GlobalProfilerSections + mSectionIndex;

  // Pop the active section
  Parent->ElapsedExclusive -= Elapsed;
  Section->ElapsedExclusive += Elapsed;
  Section->ElapsedInclusive = mPreviousElapsedInclusive + Elapsed;
  Section->Name = mSectionName;
  Section->Hits++;
}

void PrintSectionData(u64 TotalElapsed, u64 CPUFrequency) {
  for (u32 Index = 1; Index < ArrayCount(GlobalProfilerSections); Index++) {
    profiler_section *Section = &GlobalProfilerSections[Index];

    if (Section->Name) {
      printf("\t%s[%llu]: %llu, %llu(%.2f%%, %.2f%%)", Section->Name,
             Section->Hits, Section->ElapsedInclusive,
             Section->ElapsedExclusive,
             100.0 * Section->ElapsedInclusive / TotalElapsed,
             100.0 * Section->ElapsedExclusive / TotalElapsed);

      if (Section->ProcessedByteCount) {
        f64 Megabyte = 1024.0f * 1024.0f;
        f64 Gigabyte = 1024.0f * Megabyte;

        f64 Seconds = (f64)Section->ElapsedInclusive / (f64)CPUFrequency;
        f64 BytesPerSecond = (f64)Section->ProcessedByteCount / (f64)Seconds;
        f64 Megabytes = (f64)Section->ProcessedByteCount / (f64)Megabyte;
        f64 GigabytesPerSecond = BytesPerSecond / Gigabyte;

        printf(" %.3fmb at %.2fgb/s", Megabytes, GigabytesPerSecond);
      }

      putchar('\n');
    }
  }
}

#define NameConcat2(A, B) A##B
#define NameConcat(A, B) NameConcat2(A, B)
#define TimeBandwidth(NAME, BYTE_COUNT)                                        \
  profiler_trace NameConcat(Trace, __LINE__)(NAME, __COUNTER__ + 1, BYTE_COUNT)
#define TimeBlock(NAME) TimeBandwidth(NAME, 0)
#define TimeFunction TimeBlock(__func__)

#else

#define TimeBlock(...)
#define TimeBandwidth(...)
#define TimeFunction

#define PrintSectionData(...)

#endif

struct profiler {
  u64 CPUFrequency;
  u64 StartCounter;
};
static profiler GlobalProfiler;

void BeginProfile() {
  GlobalProfiler.CPUFrequency = EstimateCPUFrequency();
  GlobalProfiler.StartCounter = ReadCPUTimer();
}

void EndProfileAndPrint() {
  u64 EndCounter = ReadCPUTimer();
  u64 TotalElapsed = (EndCounter - GlobalProfiler.StartCounter);

  printf("== Profiling results (Total time: %.3fms)\n",
         1000.0 * (f64)TotalElapsed / (f64)GlobalProfiler.CPUFrequency);

  PrintSectionData(TotalElapsed, GlobalProfiler.CPUFrequency);
}
