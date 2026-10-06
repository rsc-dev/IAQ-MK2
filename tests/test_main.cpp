#include "test.h"

int g_failures = 0;
static TestCase* g_tests = nullptr;

void registerTest(TestCase* tc) {
  tc->next = g_tests;
  g_tests = tc;
}

int runAllTests() {
  int count = 0;
  for (TestCase* tc = g_tests; tc; tc = tc->next) {
    const int before = g_failures;
    tc->fn();
    ++count;
    std::printf("%s %s\n", g_failures == before ? "PASS" : "FAIL", tc->name);
  }
  std::printf("%d tests, %d failures\n", count, g_failures);
  return g_failures == 0 ? 0 : 1;
}

int main() { return runAllTests(); }
