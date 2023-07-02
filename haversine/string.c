typedef struct {
	char* Data;
	size_t Size;
} string;

string CopyString(const char* String, off_t StartOffset, size_t Count) {
	string Result = {
		.Data = (char*)malloc(sizeof(char) * (Count + 1)),
		.Size = Count
	};

	memcpy(Result.Data, String + StartOffset, Count);
	Result.Data[Count] = 0;

	return Result;
}

bool StringEqual(string A, string B) {
	if (A.Size != B.Size) return false;
	if (A.Data == B.Data) return true;

	return memcmp(A.Data, B.Data, A.Size) == 0;
}

#define STRING(LITERAL) (string) {.Data = LITERAL, .Size = sizeof(LITERAL) - 1}
