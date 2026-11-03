// tests/test_main.cpp
#include "framework.h"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

std::vector<TestCase>& testRegistry() {
  static std::vector<TestCase> reg;
  return reg;
}

int gAssertCount = 0;
static std::vector<std::string> gFailures;

void testFail(const char* file, int line, const std::string& msg) {
  const char* a = std::strrchr(file, '/');
  const char* b = std::strrchr(file, '\\');
  const char* base = a > b ? a : b;
  std::ostringstream os;
  os << (base ? base + 1 : file) << ":" << line << ": " << msg;
  gFailures.push_back(os.str());
}

int main(int argc, char** argv) {
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int passed = 0, failed = 0, skipped = 0, xfail = 0, xpass = 0;
  std::string lastSuite;
  std::vector<std::string> failedNames;

  for (TestCase& tc : testRegistry()) {
    std::string full = tc.suite + "." + tc.name;
    if (filter && full.find(filter) == std::string::npos) { ++skipped; continue; }
    if (tc.suite != lastSuite) {
      std::printf("\n%s\n", tc.suite.c_str());
      lastSuite = tc.suite;
    }
    gFailures.clear();
    tc.fn();
    if (tc.knownIssue) {
      if (gFailures.empty()) {
        std::printf("  XPASS %s (remova TEST_KNOWN_ISSUE)\n", tc.name.c_str());
        ++xpass;
      } else {
        std::printf("  KNOWN %s -- %s\n", tc.name.c_str(), tc.note.c_str());
        ++xfail;
      }
      continue;
    }
    if (gFailures.empty()) {
      std::printf("  ok   %s\n", tc.name.c_str());
      ++passed;
    } else {
      std::printf("  FAIL %s\n", tc.name.c_str());
      for (const auto& f : gFailures) std::printf("        %s\n", f.c_str());
      ++failed;
      failedNames.push_back(full);
    }
  }

  std::printf("\n%d tests, %d asserts, %d passed, %d failed",
              passed + failed + xfail + xpass, gAssertCount, passed, failed);
  if (xfail) std::printf(", %d bugs conhecidos", xfail);
  if (skipped) std::printf(", %d filtrados", skipped);
  std::printf("\n");
  for (const auto& n : failedNames) std::printf("  - %s\n", n.c_str());
  return failed ? 1 : 0;
}
