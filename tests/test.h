#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>

struct TestCase {
  const char* name;
  void (*fn)();
  TestCase* next;
};

void registerTest(TestCase* tc);
int runAllTests();
extern int g_failures;

#define TEST(name)                                                        \
  static void name();                                                     \
  static TestCase name##_case{#name, name, nullptr};                      \
  static struct name##_registrar {                                        \
    name##_registrar() { registerTest(&name##_case); }                    \
  } name##_registrar_instance;                                            \
  static void name()

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      ++g_failures;                                                       \
      std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                     \
  } while (0)

#define CHECK_EQ(a, b)                                                    \
  do {                                                                    \
    const long long _a = (long long)(a);                                  \
    const long long _b = (long long)(b);                                  \
    if (_a != _b) {                                                       \
      ++g_failures;                                                       \
      std::printf("  FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__,   \
                  __LINE__, #a, #b, _a, _b);                              \
    }                                                                     \
  } while (0)

#define CHECK_STR_EQ(a, b)                                                \
  do {                                                                    \
    const char* _a = (a);                                                 \
    const char* _b = (b);                                                 \
    if (std::strcmp(_a, _b) != 0) {                                       \
      ++g_failures;                                                       \
      std::printf("  FAIL %s:%d: %s == %s (\"%s\" vs \"%s\")\n",          \
                  __FILE__, __LINE__, #a, #b, _a, _b);                    \
    }                                                                     \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                             \
  do {                                                                    \
    const double _a = (a);                                                \
    const double _b = (b);                                                \
    if (std::fabs(_a - _b) > (eps)) {                                     \
      ++g_failures;                                                       \
      std::printf("  FAIL %s:%d: %s ~= %s (%g vs %g)\n", __FILE__,        \
                  __LINE__, #a, #b, _a, _b);                              \
    }                                                                     \
  } while (0)
