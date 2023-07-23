#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>

#include "common.hpp"

#include "os.cpp"
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

  EndProfileAndPrint();
}
