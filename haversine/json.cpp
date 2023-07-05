#define __JSON_IS_DIGIT(CH) ('0' <= (CH) && (CH) <= '9')

enum json_element_type {
	json_object,
	json_array,
	json_value
};

struct json_element;

struct json_key_value_pair {
	json_key_value_pair* Next;
	string Key;
	json_element* Value;
};

struct json_array_item {
	json_array_item* Next;
	json_element* Value;
};

struct json_element {
	json_element_type Type;

	union {
		json_key_value_pair* Object;
		json_array_item* Array;
		string Value;
	};
};

struct __json_parse_context{
	const char* Content;
	size_t ContentSize;
	size_t Offset;
};

static json_element* ParseValue(__json_parse_context* Context);
static json_element* ParseArray(__json_parse_context* Context);
static json_element* ParseObject(__json_parse_context* Context);
static void FreeJSON(json_element* Json);

static void Advance(__json_parse_context* Context) {
	if (Context->Offset != Context->ContentSize) {
		Context->Offset++;
	}
}

static char Peek(__json_parse_context* Context) {
	if (Context->Offset == Context->ContentSize) {
		return 0;
	}

	return Context->Content[Context->Offset];
}

static inline bool Expect(__json_parse_context* Context, char Expected) {
	if (Peek(Context) == Expected) {
		Advance(Context);
		return true;
	}
	return false;
}

static void EatWhitespace(__json_parse_context* Context) {
	char Peeked;

	while ((Peeked = Peek(Context)) && (Peeked == ' ' || Peeked == '\n')) {
		Advance(Context);
	}
}

static bool ParseKey(__json_parse_context* Context, string* OutKey) {
	if (!Expect(Context, '"')) return false;

	size_t StartOffset = Context->Offset;

	char Peeked = 0;
	while ((Peeked = Peek(Context)) && Peeked != '"') {
		Advance(Context);
	}

	if (!Expect(Context, '"')) return false;

	*OutKey = CopyString(Context->Content, StartOffset, Context->Offset - StartOffset - 1);

	return true;
}

static void InsertArrayItem(json_element* Json, json_element* Item) {
	json_array_item* ArrayItem = (json_array_item*)malloc(sizeof(json_array_item));

	ArrayItem->Next = Json->Array;
	ArrayItem->Value = Item;

	Json->Array = ArrayItem;
}

static json_element* ParseArray(__json_parse_context* Context) {
	if (!Expect(Context, '[')) {
		return NULL;
	}

	json_element* Json = (json_element*)malloc(sizeof(json_element));
	Json->Type = json_array;
	Json->Array = NULL;

	bool IsValid = true;
	char Peeked = 0;
	while ((Peeked = Peek(Context)) && Peeked != ']') {
		json_element* Item = ParseValue(Context);

		if (!Item) {
			IsValid = false;
			break;
		}

		InsertArrayItem(Json, Item);

		EatWhitespace(Context);
		if (Peek(Context) == ',') {
			Advance(Context);
			continue;
		}
		break;

	}
	EatWhitespace(Context);
	if (!Expect(Context, ']')) {
		IsValid = false;
	}

	if (IsValid) {
		return Json;
	} else {
		FreeJSON(Json);
		return NULL;
	}
}

static json_element* ParseValue(__json_parse_context* Context) {
	EatWhitespace(Context);

	json_element* Result = NULL;

	char PeekedCharacter = Peek(Context);

	if (PeekedCharacter == '[') {
		Result = ParseArray(Context);
	} else if (PeekedCharacter == '{') {
		Result = ParseObject(Context);
	} else {
		size_t StartOffset = Context->Offset;

		char Character = 0;

		if (Peek(Context) == '-') {
			Advance(Context);
		}

		while ((Character = Peek(Context)) && __JSON_IS_DIGIT(Character)) {
			Advance(Context);
		}

		if (Character == '.') {
			Advance(Context);
			while ((Character = Peek(Context)) && __JSON_IS_DIGIT(Character)) {
				Advance(Context);
			}
		}

		json_element* Json = (json_element*)malloc(sizeof(json_element));
		Json->Type = json_value;
		Json->Value = CopyString(Context->Content, StartOffset, Context->Offset - StartOffset);

		Result = Json;
	}

	return Result;
}

static inline void InsertKeyValue(json_element* Json, string Key, json_element* Value) {
	json_key_value_pair* Pair = (json_key_value_pair*)malloc(sizeof(json_key_value_pair));

	Pair->Key = Key;
	Pair->Value = Value;

	Pair->Next = Json->Object;
	Json->Object = Pair;
}

static json_element* ParseObject(__json_parse_context* Context) {
	EatWhitespace(Context);

	if (!Expect(Context, '{')) return NULL;

	json_element* Json = (json_element*)malloc(sizeof(json_element));
	Json->Type = json_object;
	Json->Object = NULL;

	bool IsValid = true;

	do {
		EatWhitespace(Context);

		string Key;
		if (!ParseKey(Context, &Key)) { IsValid = false; break; }

		EatWhitespace(Context);
		if (!Expect(Context, ':')) { IsValid = false; break; }

		json_element* Value = ParseValue(Context);

		if (!Value) { IsValid = false; break; }

		InsertKeyValue(Json, Key, Value);

		EatWhitespace(Context);

		if (Peek(Context) == ',') {
			Advance(Context);
			continue;
		}

		break;
	} while (1);

	EatWhitespace(Context);

	if (!Expect(Context, '}')) {
		IsValid = false;
	}

	if (IsValid) {
		return Json;
	} else {
		FreeJSON(Json);
		return NULL;
	}
};

static json_element* ParseJSON(const char* Content, size_t Size) {
	__json_parse_context Context = {
		.Content = Content,
		.ContentSize = Size
	};

	return ParseObject(&Context);
}

static void FreeJSON(json_element* Json) {
	assert(Json != NULL);

	switch (Json->Type) {
		case json_value: 
			{
				free((void*)(Json->Value.Data));
			}
			break;
		case json_array:
			{
				for (json_array_item* Item = Json->Array; Item; ) {
					FreeJSON(Item->Value);

					json_array_item* Next = Item->Next;
					free(Item);

					Item = Next;
				}
			}
			break;
		case json_object:
			{
				for (json_key_value_pair* Pair = Json->Object; Pair; ) {
					free((void*)Pair->Key.Data);
					FreeJSON(Pair->Value);

					json_key_value_pair* Next = Pair->Next;
					free(Pair);

					Pair = Next;
				}
			}
			break;
	}

	free(Json);
}

json_element* GetKey(json_element* Json, string Key) {
	TimeFunction;
	json_element* Result = NULL;

	if (Json->Type == json_object) {
		for (json_key_value_pair* Pair = Json->Object; Pair; Pair = Pair->Next) {
			if (StringEqual(Pair->Key, Key)) {
				Result = Pair->Value;
				break;
			}
		}
	}

	return Result;
}

typedef struct {
	json_array_item* CurrentItem;
} json_array_iterator;

json_array_iterator MakeJSONArrayIterator(json_element* Json) {
	json_array_iterator Result = {0};

	if (Json->Type == json_array) {
		Result.CurrentItem = Json->Array;
	}

	return Result;
}

json_element* Next(json_array_iterator* Iter) {
	json_element* Result = NULL;

	if (Iter->CurrentItem)  {
		Result = Iter->CurrentItem->Value;
		Iter->CurrentItem = Iter->CurrentItem->Next;
	}

	return Result;
}

f64 ConvertJSONValueToF64(json_element* Json) {
	assert(Json->Type == json_value);

	return atof(Json->Value.Data);
}
