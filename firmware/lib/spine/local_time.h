#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace spine {

const char* posixTz(const std::string& iana);

int64_t localMidnightMs(const char* posix, int64_t nowMs);

int nowMinutes(const char* posix, int64_t nowMs);

struct Window {
  int startMin;
  int endMin;
};
int toMin(const char* hhmm);

bool eligibility(const std::vector<Window>& schedule, const char* posix, int64_t nowMs);

}
