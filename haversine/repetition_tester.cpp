#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "common.hpp"
#include "os.cpp"

struct buffer {
  u8 *Data;
  size_t Size;
};

enum metric {
  Metric_TestCount = 0,
  Metric_ByteCount,
  Metric_CPUTimer,
  Metric_MemPageFaults,

  Metric_COUNT
};

struct test_metrics {
  u64 M[Metric_COUNT];
};

struct test_results {
  test_metrics Min;
  test_metrics Max;
  test_metrics Total;
};

enum test_mode {
  TestMode_Uninitialized = 0,
  TestMode_Running,
  TestMode_Completed
};
struct test_context {
  test_mode Mode;

  u64 TargetTime;

  u64 TotalBytesProcessed;
  u64 TestsStartTime;

  test_metrics AccumulatedOnThisTest;
  test_results Results;
};

enum allocation_type { AllocType_none = 0, AllocType_malloc, AllocType_COUNT };

struct read_parameters {
  allocation_type AllocType;
  buffer Dest;
  const char *Filepath;
};

typedef void read_file_fn(test_context *, read_parameters *);

struct test_case {
  const char *Name;
  read_file_fn *Func;
  allocation_type AllocType;
};

static void CountBytes(test_context *Context, u64 Count) {
  test_metrics *Accum = &Context->AccumulatedOnThisTest;
  Accum->M[Metric_ByteCount] += Count;
}

static void BeginTime(test_context *Context) {
  test_metrics *Accum = &Context->AccumulatedOnThisTest;

  Accum->M[Metric_CPUTimer] -= ReadCPUTimer();
  Accum->M[Metric_MemPageFaults] -= ReadOSPageFaultCount();
}

static void EndTime(test_context *Context) {
  test_metrics *Accum = &Context->AccumulatedOnThisTest;

  Accum->M[Metric_CPUTimer] += ReadCPUTimer();
  Accum->M[Metric_MemPageFaults] += ReadOSPageFaultCount();
}

static buffer AllocateBuffer(size_t Size) {
  return (buffer){.Data = (u8 *)malloc(Size), .Size = Size};
}

static void FreeBuffer(buffer *Buffer) {
  free(Buffer->Data);
  Buffer->Data = NULL;
  Buffer->Size = 0;
}

static void HandleAllocation(read_parameters *Params, buffer *Buffer) {
  switch (Params->AllocType) {
  case AllocType_none:
    break;
  case AllocType_malloc:
    *Buffer = AllocateBuffer(Params->Dest.Size);
    break;
  default:
    abort();
  }
}

static void HandleDeallocation(read_parameters *Params, buffer *Buffer) {
  switch (Params->AllocType) {
  case AllocType_none:
    break;
  case AllocType_malloc:
    FreeBuffer(Buffer);
    break;
  default:
    abort();
  }
}

static bool IsTesting(test_context *Context) {
  u64 CurrentTime = ReadCPUTimer();

  if (Context->Mode == TestMode_Uninitialized) {
    Context->Mode = TestMode_Running;
    test_results *R = &Context->Results;
    R->Min.M[Metric_CPUTimer] = ~0;
    Context->TestsStartTime = CurrentTime;
  } else if (Context->Mode == TestMode_Running) {
    test_results *R = &Context->Results;
    R->Total.M[Metric_TestCount] += 1;

    test_metrics Accum = Context->AccumulatedOnThisTest;

    for (u32 Metric = 0; Metric < Metric_COUNT; ++Metric) {
      R->Total.M[Metric] += Accum.M[Metric];
    }

    if (R->Max.M[Metric_CPUTimer] < Accum.M[Metric_CPUTimer]) {
      R->Max = Accum;
    }

    if (R->Min.M[Metric_CPUTimer] > Accum.M[Metric_CPUTimer]) {
      R->Min = Accum;
      Context->TestsStartTime = CurrentTime;
    }

    Context->AccumulatedOnThisTest = {};

    if (CurrentTime - Context->TestsStartTime > Context->TargetTime) {
      Context->Mode = TestMode_Completed;
    }
  }

  return (Context->Mode == TestMode_Running);
}

void ReadEntireFile_Fread(test_context *Context, read_parameters *Params) {
  while (IsTesting(Context)) {
    FILE *File = fopen(Params->Filepath, "rb");

    if (File) {
      buffer Buffer = Params->Dest;
      HandleAllocation(Params, &Buffer);

      BeginTime(Context);
      int Result = fread(Buffer.Data, sizeof(u8), Buffer.Size, File);
      EndTime(Context);

      if (Result == Buffer.Size) {
        CountBytes(Context, Buffer.Size);
      }

      HandleDeallocation(Params, &Buffer);
      fclose(File);
    }
  }
}

static void ReadEntireFile_ReadSyscall(test_context *Context,
                                       read_parameters *Params) {
  while (IsTesting(Context)) {
    int FileDescriptor = open(Params->Filepath, O_RDONLY);

    if (FileDescriptor >= 0) {

      buffer Buffer = Params->Dest;
      HandleAllocation(Params, &Buffer);

      BeginTime(Context);
      int Result = read(FileDescriptor, Buffer.Data, Buffer.Size);
      EndTime(Context);

      if (Result == Buffer.Size) {
        CountBytes(Context, Buffer.Size);
      }

      HandleDeallocation(Params, &Buffer);
      close(FileDescriptor);
    }
  }
}

static const char *DescribeAllocationType(allocation_type AllocType) {
  switch (AllocType) {
  case AllocType_none:
    return "";
  case AllocType_malloc:
    return "malloc";
  default:
    abort();
    return "";
  }
}

static void PrintTestMetrics(const char *Label, test_metrics Metrics,
                             u64 CPUTimerFrequency) {
  u64 TestCount = Metrics.M[Metric_TestCount];
  f64 Divisor = TestCount ? (f64)TestCount : 1.0;

  f64 R[Metric_COUNT];
  for (u32 Index = 0; Index < Metric_COUNT; ++Index) {
    R[Index] = (f64)Metrics.M[Index] / Divisor;
  }

  printf("%s: %.0f", Label, R[Metric_CPUTimer]);

  if (CPUTimerFrequency) {
    f64 Seconds = R[Metric_CPUTimer] / CPUTimerFrequency;
    f64 Milliseconds = 1000.0 * Seconds;
    printf(" (%fms)", Milliseconds);

    if (R[Metric_ByteCount]) {
      f64 Gigabyte = 1024.0 * 1024.0 * 1024.0;
      f64 Bandwidth = R[Metric_ByteCount] / (Gigabyte * Seconds);

      printf(" %fgb/s", Bandwidth);
    }
  }

  if (R[Metric_MemPageFaults] > 0.0) {
    printf(" PF: %0.4f (%0.4fkb/fault)", R[Metric_MemPageFaults],
           R[Metric_ByteCount] / (R[Metric_MemPageFaults] * 1024.0));
  }
}

static void PrintTestResults(test_results Results, u64 CPUTimerFrequency) {
  PrintTestMetrics("Min", Results.Min, CPUTimerFrequency);
  printf("\n");
  PrintTestMetrics("Avg", Results.Total, CPUTimerFrequency);
  printf("\n");
  PrintTestMetrics("Max", Results.Max, CPUTimerFrequency);
  printf("\n");
}

int main(int ArgCount, char *Args[]) {
  u64 RunCounter = 0;

  u64 CPUTimerFrequency = EstimateCPUFrequency();

  const char *Filepath = Args[1];

  struct stat FileStat;
  stat(Filepath, &FileStat);

  u64 FileSize = FileStat.st_size;

  buffer FixedBuffer = AllocateBuffer(1024 * 1024 * 1024);

  for (;;) {
    RunCounter++;
    test_case Tests[4] = {
        {"read", &ReadEntireFile_ReadSyscall, AllocType_none},
        {"read", &ReadEntireFile_ReadSyscall, AllocType_malloc},
        {"fread", &ReadEntireFile_Fread, AllocType_none},
        {"fread", &ReadEntireFile_Fread, AllocType_malloc}};

    printf("=====> Run #%llu\n", RunCounter);
    for (off_t Index = 0; Index < ArrayCount(Tests); ++Index) {

      test_case *Test = &Tests[Index];

      read_parameters Params = {};
      Params.AllocType = Test->AllocType;
      Params.Filepath = Filepath;
      Params.Dest = FixedBuffer;
      Params.Dest.Size = FileSize;

      test_context Context = {};
      Context.TargetTime = 10 * CPUTimerFrequency; // Try for ten seconds

      Test->Func(&Context, &Params);

      printf("\n--- %s%s%s ---\n", DescribeAllocationType(Params.AllocType),
             Params.AllocType ? " + " : "", Test->Name);
      PrintTestResults(Context.Results, CPUTimerFrequency);
    }
    printf("\n");
  }

  return 0;
}
