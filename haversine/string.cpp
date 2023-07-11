struct string {
  const char *Data;
  size_t Size;
};

string CopyString(const char *String, off_t StartOffset, size_t Count) {
  char *Data = (char *)malloc(sizeof(char) * (Count + 1));
  memcpy(Data, String + StartOffset, Count);
  Data[Count] = 0;

  return {.Data = Data, .Size = Count};
}

bool StringEqual(string A, string B) {
  if (A.Size != B.Size)
    return false;
  if (A.Data == B.Data)
    return true;

  return memcmp(A.Data, B.Data, A.Size) == 0;
}

#define STRING(LITERAL)                                                        \
  (string) { .Data = LITERAL, .Size = sizeof(LITERAL) - 1 }
