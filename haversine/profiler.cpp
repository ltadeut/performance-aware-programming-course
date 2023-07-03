#include <sys/time.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

struct profiler_section {
	profiler_section* Next;
	const char* Name;
	f64 Runtime;
	u64 Hits;
};

static profiler_section RootSection;
static profiler_section* LastSection;
static profiler_section* ActiveSection;
static u64 ProfilerStartTime;

profiler_section* LookupSectionByName(const char* SectionName) {
	profiler_section* Section = RootSection.Next;

	while (Section) {
		if (strcmp(SectionName, Section->Name) == 0) break;
		Section = Section->Next;
	}

	return Section;
}

class profiler_trace {
	private:
		profiler_section* mSection;
		u64 mStartTime;
	public:
		profiler_trace(const char* SectionName);
		~profiler_trace();
};

profiler_trace::profiler_trace(const char* SectionName) {
	mSection = LookupSectionByName(SectionName);

	if (!mSection) {
		mSection = (profiler_section*)malloc(sizeof(profiler_section));
		mSection->Next = nullptr;
		mSection->Name = SectionName;
		mSection->Hits = 0;
		mSection->Runtime = 0.0;

		LastSection->Next = mSection;
		LastSection = mSection;
	}

	mSection->Hits++;
	mStartTime = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
}

profiler_trace::~profiler_trace() {
	u64 EndTime = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);

	mSection->Runtime += (EndTime - mStartTime) / 1000.0;
}

#define __TRACE(NAME) profiler_trace Trace##__LINE__(NAME)
#define _STR(X) #X
#define TimeBlock(BLOCK_NAME) __TRACE(BLOCK_NAME)
#define TimeFunction TimeBlock(__func__)


void BeginProfile() {
	RootSection.Next = nullptr;
	LastSection = &RootSection;
	ActiveSection = nullptr;

	ProfilerStartTime = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);
}

void EndProfileAndPrint() {
	u64 EndTime = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);

	f64 TotalRuntime = (EndTime - ProfilerStartTime) / 1000.0;

	printf("== Profiling results (Total time: %.3fms)\n", TotalRuntime);

	for (profiler_section* Section = RootSection.Next;
			Section;
	    ) {

		printf("\t%s[%llu]: %.3fms (%.2f%%)\n", Section->Name, Section->Hits, Section->Runtime, ((f64)Section->Runtime / (f64)TotalRuntime) * 100.0);

		profiler_section* NextSection = Section->Next;
		free(Section);

		Section = NextSection;
	}
}
