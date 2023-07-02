#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef double f64;

#include "string.c"
#include "json.c"
#include "haversine_formula.c"

#define EARTH_RADIUS 6372.8

typedef struct {
	unsigned char* Data;
	size_t Size;
} buffer;

typedef struct {
	buffer Contents;
	bool IsValid;
} memory_mapped_file;

memory_mapped_file OpenMemoryMappedFile(const char* FileName) {
	memory_mapped_file Result = {0};

	int FileDescriptor = open(FileName, O_RDONLY);
	if (FileDescriptor != -1) {
		struct stat FileStats = {0};
		if (fstat(FileDescriptor, &FileStats) == 0) {

			size_t FileSize = FileStats.st_size;
			// NOTE(Lucas): On macos the flags parameter has to either have MAP_PRIVATE or MAP_SHARED set.
			// without having one of those set, mmap will fail.
			// 
			// This bit of information was hiding at the 'compatibility' section of the man page (man 2 mmap).
			void* Mapping = mmap(0, FileSize, PROT_READ, MAP_PRIVATE | MAP_FILE, FileDescriptor, 0);

			if (Mapping != MAP_FAILED) {
				Result.Contents = (buffer) {
					.Data = Mapping,
						.Size = FileSize
				};

				Result.IsValid = true;
			}
		}

		close(FileDescriptor);
	}

	return Result;
}


bool CloseMemoryMappedFile(memory_mapped_file* File) {
	if (File->IsValid) {
		if (munmap(File->Contents.Data, File->Contents.Size) == 0) {
			File->IsValid = false;
			File->Contents.Data = NULL;
			File->Contents.Size = 0;
			return true;
		}
	}

	return false;
}


int main(int CommandLineArgumentsCount, char* CommandLineArguments[]) {
	const char* FileName = CommandLineArguments[1];
	memory_mapped_file File = OpenMemoryMappedFile(FileName);

	if (!File.IsValid) {
		fprintf(stderr, "Failed to open the input file: %s\n", FileName);
		return 1;
	}

	json_element* JsonData = ParseJSON((char*)File.Contents.Data, File.Contents.Size);

	if (JsonData)  {
		json_element* PairsData = GetKey(JsonData, STRING("pairs"));

		if (PairsData) {
			int Count = 0;
			f64 HaversineDistanceSum = 0;
			json_array_iterator Iter = MakeJSONArrayIterator(PairsData);

			for (json_element* Item = Next(&Iter); Item; Item = Next(&Iter)) {
				json_element* X0Value = GetKey(Item, STRING("x0"));
				f64 X0 = ConvertJSONValueToF64(X0Value);

				json_element* Y0Value = GetKey(Item, STRING("y0"));
				f64 Y0 = ConvertJSONValueToF64(Y0Value);

				json_element* X1Value = GetKey(Item, STRING("x1"));
				f64 X1 = ConvertJSONValueToF64(X1Value);

				json_element* Y1Value = GetKey(Item, STRING("y1"));
				f64 Y1 = ConvertJSONValueToF64(Y1Value);


				HaversineDistanceSum += ReferenceHaversine(X0, Y0, X1, Y1, EARTH_RADIUS);
				Count++;
			}

			f64 AverageHaversineDistance = HaversineDistanceSum / (f64)Count;

			f64 ExpectedAverageHaversineDistance = 0;

			printf("Number of Coordinate Pairs: %d\nAverage Haversine Distance: %f\n", Count, AverageHaversineDistance);
		}
	}

	CloseMemoryMappedFile(&File);

	return 0;
}
