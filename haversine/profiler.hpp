struct profiler_section {
	profiler_section* Next;
	const char* Name;
	f64 Runtime;
	u64 Hits;
};

class profiler_trace {
	private:
		profiler_section* mSection;
		u64 mStartTime;
	public:
		profiler_trace(const char* SectionName);
		~profiler_trace();
};
