#include <sys/time.h>

struct profiler_section {
	profiler_section* Next;
	const char* Name;
	u64 Elapsed;
	u64 Hits;
};

class profiler_trace {
	private:
		profiler_section* mSection;
		u64 mStartCounter;
	public:
		profiler_trace(const char* SectionName);
		~profiler_trace();
};

struct profiler {
	profiler_section RootSection;
	profiler_section* LastSection;
	profiler_section* ActiveSection;
	u64 StartCounter;
	u64 EndCounter;
	u64 CPUFrequency;
};

static profiler GlobalProfiler;

static profiler_section* LookupSectionByName(const char* SectionName) {
	profiler_section* Section = GlobalProfiler.RootSection.Next;

	while (Section) {
		if (strcmp(SectionName, Section->Name) == 0) break;
		Section = Section->Next;
	}

	return Section;
}

static u64 ReadCPUVirtualCounter() {
	u64 Result;

	asm volatile ("isb;\n\tmrs %0, cntvct_el0" : "=r" (Result) :: "memory");

	return Result;
}

static u64 ReadCPUVirtualCounterFrequency() {
	u64 Result;

	asm volatile ("mrs %0, cntfrq_el0" : "=r" (Result) :: "memory");

	return Result;
}

profiler_trace::profiler_trace(const char* SectionName) {
	mSection = LookupSectionByName(SectionName);

	if (!mSection) {
		mSection = (profiler_section*)malloc(sizeof(profiler_section));
		mSection->Next = nullptr;
		mSection->Name = SectionName;
		mSection->Hits = 0;
		mSection->Elapsed = 0;

		GlobalProfiler.LastSection->Next = mSection;
		GlobalProfiler.LastSection = mSection;
	}

	mStartCounter = ReadCPUVirtualCounter();
}

profiler_trace::~profiler_trace() {
	u64 EndCounter = ReadCPUVirtualCounter();

	mSection->Hits++;
	mSection->Elapsed += (EndCounter - mStartCounter);
}

#define __TRACE(NAME) profiler_trace Trace##__LINE__(NAME)
#define TimeBlock(BLOCK_NAME) __TRACE(BLOCK_NAME)
#define TimeFunction TimeBlock(__func__)

void BeginProfile() {
	GlobalProfiler.RootSection.Next = nullptr;
	GlobalProfiler.LastSection = &GlobalProfiler.RootSection;
	GlobalProfiler.StartCounter = ReadCPUVirtualCounter();
	GlobalProfiler.CPUFrequency = ReadCPUVirtualCounterFrequency();
}

void EndProfileAndPrint() {
	GlobalProfiler.EndCounter = ReadCPUVirtualCounter();

	u64 TotalElapsed = (GlobalProfiler.EndCounter - GlobalProfiler.StartCounter);

	printf("== Profiling results (Total ticks: %llu)\n", TotalElapsed);

	for (profiler_section* Section = GlobalProfiler.RootSection.Next;
			Section;
	    ) {
		printf("\t%s[%llu]: %llu(%.2f%%)\n", Section->Name, Section->Hits, Section->Elapsed, ((f64)Section->Elapsed / (f64)TotalElapsed) * 100.0);

		profiler_section* NextSection = Section->Next;
		free(Section);

		Section = NextSection;
	}
}
