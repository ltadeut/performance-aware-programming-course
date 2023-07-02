#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define NUMBER_OF_CLUSTERS 64
#define EARTH_RADIUS 6372.8

typedef double f64;

typedef struct {
	f64 X0;
	f64 Y0;
	f64 X1;
	f64 Y1;
} coordinates_pair;

static f64 Square(f64 A)
{
    f64 Result = (A*A);
    return Result;
}

static f64 RadiansFromDegrees(f64 Degrees)
{
    f64 Result = 0.01745329251994329577f * Degrees;
    return Result;
}

// NOTE(casey): EarthRadius is generally expected to be 6372.8
static f64 ReferenceHaversine(coordinates_pair* Pair, f64 EarthRadius)
	
{
    f64 lat1 = Pair->Y0;
    f64 lat2 = Pair->Y1;
    f64 lon1 = Pair->X0;
    f64 lon2 = Pair->X1;
    
    f64 dLat = RadiansFromDegrees(lat2 - lat1);
    f64 dLon = RadiansFromDegrees(lon2 - lon1);
    lat1 = RadiansFromDegrees(lat1);
    lat2 = RadiansFromDegrees(lat2);
    
    f64 a = Square(sin(dLat/2.0)) + cos(lat1)*cos(lat2)*Square(sin(dLon/2));
    f64 c = 2.0*asin(sqrt(a));
    
    f64 Result = EarthRadius * c;
    
    return Result;
}

static coordinates_pair GenerateRandomPair() {
	return (coordinates_pair) {
		.X0 = ((f64)rand() / RAND_MAX - 0.5) * 2 * 180,
			.Y0 = ((f64)rand() / RAND_MAX - 0.5) * 2 * 90,
			.X1 = ((f64)rand() / RAND_MAX - 0.5) * 2 * 180,
			.Y1 = ((f64)rand() / RAND_MAX - 0.5) * 2 * 90,
	};
}

static f64 GenerateClusterPairs(coordinates_pair* ArrayOfPairs, int NumberOfPairs) {
	f64 HaversineSum = 0;

	// Pick a random center
	f64 CenterX = ((f64)rand() / RAND_MAX - 0.5) * 2 * 180;
	f64 CenterY = ((f64)rand() / RAND_MAX - 0.5) * 2 * 90;

	// Pick a random radius
	f64 Radius = ((f64)rand() / RAND_MAX) * 25; // Clusters will be at most 50 degrees in diameter

	for (int i = 0; i < NumberOfPairs; i++) {
		ArrayOfPairs[i].X0 = ((f64)rand() / RAND_MAX - 0.5) * 2 * Radius + CenterX;
		ArrayOfPairs[i].Y0 = ((f64)rand() / RAND_MAX - 0.5) * 2 * Radius + CenterY;

		ArrayOfPairs[i].X1 = ((f64)rand() / RAND_MAX - 0.5) * 2 * Radius + CenterX;
		ArrayOfPairs[i].Y1 = ((f64)rand() / RAND_MAX - 0.5) * 2 * Radius + CenterY;

		HaversineSum += ReferenceHaversine(&ArrayOfPairs[i], EARTH_RADIUS);
	}

	return HaversineSum;
}

static void PrintPair(FILE* OutputFile, coordinates_pair Pair, bool ShouldPrintCommaAtTheEnd) {
	fprintf(OutputFile, "{\"x0\": %f, \"y0\": %f, \"x1\": %f, \"y1\": %f}", Pair.X0, Pair.Y0, Pair.X1, Pair.Y1);
	if (ShouldPrintCommaAtTheEnd) {
		fprintf(OutputFile, ",");
	}

	fprintf(OutputFile, "\n");
}

static void PrintUsage(const char*  ProgramName) {
	fprintf(stderr, "Usage: %s MODE SEED COUNT\n\n", ProgramName);
	fprintf(stderr, "MODE\tHow to generate the random coordinate pairs. Accepted values: 'uniform' or 'cluster'.\n");
	fprintf(stderr, "SEED\tAn integer to be used to initialize the PRNG.\n");
	fprintf(stderr, "COUNT\tNumber of coordinate pairs to generate\n");
}

typedef enum {
	mode_invalid = 0,
	mode_uniform,
	mode_cluster
} mode;

typedef struct {
	mode Mode;
	int NumberOfCoordinatePairs;
	int Seed;
	bool IsValid;
} options;

static options ParseCommandLineOptions(int CommandLineArgumentsCount, char** CommandLineArguments) {
	options Result = {0};

	if (CommandLineArgumentsCount == 4) {
		if (strncmp(CommandLineArguments[1], "uniform", 7) == 0) {
			Result.Mode = mode_uniform;
		} else if (strncmp(CommandLineArguments[1], "cluster", 7) == 0) {
			Result.Mode = mode_cluster;
		}


		sscanf(CommandLineArguments[2], "%d", &Result.Seed);
		sscanf(CommandLineArguments[3], "%d", &Result.NumberOfCoordinatePairs);

		Result.IsValid = true;
	}

	return Result;
}

static void MakeOutputFileName(char* Buffer, size_t BufferSize, mode Mode, int NumberOfCoordinatePairs) {
	snprintf(Buffer, BufferSize, "data_%s_%d.json", (Mode == mode_cluster) ?  "cluster" : "uniform", NumberOfCoordinatePairs);
}

static void MakeHaversineResultFileName(char* Buffer, size_t BufferSize, mode Mode, int NumberOfCoordinatePairs) {
	snprintf(Buffer, BufferSize, "data_%s_%d.f64", (Mode == mode_cluster) ?  "cluster" : "uniform", NumberOfCoordinatePairs);
}

static char TempBuf[255];

int main(int CommandLineArgumentsCount, char* CommandLineArguments[]) {
	options Options = ParseCommandLineOptions(CommandLineArgumentsCount, CommandLineArguments);

	if (!Options.IsValid) {
		PrintUsage(CommandLineArguments[0]);
		return 1;
	}

	fprintf(stderr, "Generating %d coordinate pairs ... ", Options.NumberOfCoordinatePairs);

	srand(Options.Seed);

	MakeOutputFileName(TempBuf, sizeof(TempBuf), Options.Mode, Options.NumberOfCoordinatePairs);

	f64 HaversineSum = 0;

	{
		FILE* OutputFile = fopen(TempBuf, "wt");
		fprintf(OutputFile, "{\"pairs\": [\n");

		if (Options.Mode == mode_uniform) {
			for (int i = 1; i <= Options.NumberOfCoordinatePairs; i++) {
				coordinates_pair Pair = GenerateRandomPair();
				HaversineSum += ReferenceHaversine(&Pair, EARTH_RADIUS);
				PrintPair(OutputFile, Pair, i != Options.NumberOfCoordinatePairs);
			}
		} else {
			int PairsPerCluster = Options.NumberOfCoordinatePairs / NUMBER_OF_CLUSTERS;
			int RemainderPairs = Options.NumberOfCoordinatePairs % NUMBER_OF_CLUSTERS;

			coordinates_pair* Pairs = (coordinates_pair*) malloc(sizeof(coordinates_pair) * (PairsPerCluster + RemainderPairs));

			for (int  i = 1; i <= NUMBER_OF_CLUSTERS; i++) {
				int NumberOfPairsInCluster = PairsPerCluster + ((i == NUMBER_OF_CLUSTERS) ? RemainderPairs : 0);
				HaversineSum += GenerateClusterPairs(Pairs, NumberOfPairsInCluster);

				for (int j = 1; j <= NumberOfPairsInCluster; j++) {
					PrintPair(OutputFile, Pairs[j-1], (i != NUMBER_OF_CLUSTERS) && (j != NumberOfPairsInCluster));
				}
			}
		}

		fprintf(OutputFile, "]}\n");
		fclose(OutputFile);
	}


	f64 HaversineAverage = HaversineSum / (f64) Options.NumberOfCoordinatePairs;
	MakeHaversineResultFileName(TempBuf, sizeof(TempBuf), Options.Mode, Options.NumberOfCoordinatePairs);

	FILE *HaversineResultFile = fopen(TempBuf, "wt");
	fprintf(HaversineResultFile, "%f\n", HaversineAverage);
	fclose(HaversineResultFile);

	fprintf(stderr, "DONE\n");

	fprintf(stderr, "Random seed: %d\nNumber of coordinate pairs: %d\nMethod: %s\nHaversine average: %f\n", Options.Seed, Options.NumberOfCoordinatePairs, (Options.Mode == mode_cluster) ? "cluster" : "uniform", HaversineAverage);
	return 0;
}
