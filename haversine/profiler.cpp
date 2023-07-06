struct profiler_section {
	const char* Name;
	u64 Elapsed;
	u64 ChildrenElapsed;
	u64 Hits;
};

class profiler_trace {
	private:
		u64 mStartCounter;
		u32 mSectionRef;
	public:
		profiler_trace(const char* SectionName);
		~profiler_trace();
};

struct profiler {
	profiler_section Sections[4096];
	u32 SectionsCount;
	u32 ActiveSectionsStack[4096];
	u32 ActiveSectionsCount;
	u64 StartCounter;
	u64 EndCounter;
	u64 CPUFrequency;
};

static profiler GlobalProfiler;

static u32 LookupSectionByName(const char* SectionName) {

	for (u32 Index = 1; Index <= GlobalProfiler.SectionsCount; Index++) {
		if (GlobalProfiler.Sections[Index].Name == SectionName) {
			return Index;
		}
	}

	return 0;
}

#if __arm__ || __aarch64__
static u64 ReadCPUTimer() {
	u64 Result;

	asm volatile ("isb;\n\tmrs %0, cntvct_el0" : "=r" (Result) :: "memory");

	return Result;
}

static u64 EstimateCPUFrequency() {
	u64 Result;

	asm volatile ("mrs %0, cntfrq_el0" : "=r" (Result) :: "memory");

	return Result;
}

#elif _WIN64

#include <intrin.h>
#include <windows.h>

static u64 GetOSTimerFreq(void)
{
	LARGE_INTEGER Freq;
	QueryPerformanceFrequency(&Freq);
	return Freq.QuadPart;
}

static u64 ReadOSTimer(void)
{
	LARGE_INTEGER Value;
	QueryPerformanceCounter(&Value);
	return Value.QuadPart;
}

static u64 ReadCPUTimer() {
		return __rdtsc();
}

static u64 EstimateCPUFrequency(void)
{
	u64 MillisecondsToWait = 100;
	u64 OSFreq = GetOSTimerFreq();

	u64 CPUStart = ReadCPUTimer();
	u64 OSStart = ReadOSTimer();
	u64 OSEnd = 0;
	u64 OSElapsed = 0;
	u64 OSWaitTime = OSFreq * MillisecondsToWait / 1000;
	while(OSElapsed < OSWaitTime)
	{
		OSEnd = ReadOSTimer();
		OSElapsed = OSEnd - OSStart;
	}
	
	u64 CPUEnd = ReadCPUTimer();
	u64 CPUElapsed = CPUEnd - CPUStart;
	
	u64 CPUFreq = 0;
	if(OSElapsed)
	{
		CPUFreq = OSFreq * CPUElapsed / OSElapsed;
	}
	
	return CPUFreq;
}
#endif

profiler_trace::profiler_trace(const char* SectionName) {

	mSectionRef = LookupSectionByName(SectionName);
	if (mSectionRef == 0) {
		mSectionRef = ++GlobalProfiler.SectionsCount;
		GlobalProfiler.Sections[mSectionRef].Name = SectionName;
	}

	GlobalProfiler.ActiveSectionsStack[++GlobalProfiler.ActiveSectionsCount] = mSectionRef;
	mStartCounter = ReadCPUTimer();
}

profiler_trace::~profiler_trace() {
	u64 EndCounter = ReadCPUTimer();
	u64 Elapsed = EndCounter - mStartCounter;

	// Pop the active section
	GlobalProfiler.Sections[mSectionRef].Hits++;
	u32 ParentRef = GlobalProfiler.ActiveSectionsStack[--GlobalProfiler.ActiveSectionsCount];

	bool Recursive = false;
	for (u32 Index = GlobalProfiler.ActiveSectionsCount; Index; Index--) {
		u32 Ref = GlobalProfiler.ActiveSectionsStack[Index];

		if (Ref == mSectionRef) {
			Recursive = true;
		}
	}

	if (mSectionRef != ParentRef) GlobalProfiler.Sections[ParentRef].ChildrenElapsed += Elapsed;
	if (!Recursive) GlobalProfiler.Sections[mSectionRef].Elapsed += Elapsed;
}

#define NameConcat2(A, B) A##B
#define NameConcat(A, B) NameConcat2(A, B)
#define TimeBlock(NAME) profiler_trace NameConcat(Trace, __LINE__)(NAME)
#define TimeFunction TimeBlock(__func__)

void BeginProfile() {
	memset(GlobalProfiler.Sections, 0, sizeof(GlobalProfiler.Sections));
	GlobalProfiler.SectionsCount = 0;

	memset(GlobalProfiler.ActiveSectionsStack, 0, sizeof(GlobalProfiler.ActiveSectionsStack));
	GlobalProfiler.ActiveSectionsCount = 0;

	GlobalProfiler.CPUFrequency = EstimateCPUFrequency();
	GlobalProfiler.StartCounter = ReadCPUTimer();
}

void EndAndPrintProfile() {
	GlobalProfiler.EndCounter = ReadCPUTimer();

	u64 TotalElapsed = (GlobalProfiler.EndCounter - GlobalProfiler.StartCounter);

	printf("== Profiling results (Total ticks: %llu)\n", TotalElapsed);

	for (u32 Index = 1; Index <= GlobalProfiler.SectionsCount; Index++) {
		profiler_section* Section = &GlobalProfiler.Sections[Index];

		u64 ElapsedMinusChildren = Section->Elapsed - Section->ChildrenElapsed;
		f64 ElapsedPercentage =((f64)Section->Elapsed / (f64)TotalElapsed) * 100.0;
		f64 ElapsedMinusChildrenPercentage =((f64)ElapsedMinusChildren / (f64)TotalElapsed) * 100.0;

		printf("\t%s[%llu]: %llu, %llu(%.2f%%, %.2f%%)\n", Section->Name, Section->Hits, Section->Elapsed, ElapsedMinusChildren, ElapsedPercentage, ElapsedMinusChildrenPercentage);

	}
}
