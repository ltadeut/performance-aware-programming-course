#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cstring>
#include <cstdio>

#include <unistd.h>

#include "common.hpp"

#include "profiler.cpp"


void B();

void A(bool CallB) {

	TimeFunction;

	if (CallB) {
		B();
	}
}

void B() {
	TimeFunction;

	sleep(10);

	A(false);
}


int main() {
	BeginProfile();
	
	A(true);

	EndAndPrintProfile();
}
