#include "local_time.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace spine {

namespace {
struct TzEntry {
  const char* iana;
  const char* posix;
};
const TzEntry kTzTable[] = {
    {"UTC", "UTC0"},
    {"America/Toronto", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Montreal", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Phoenix", "MST7"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Vancouver", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Halifax", "AST4ADT,M3.2.0,M11.1.0"},
    {"America/St_Johns", "NST3:30NDT,M3.2.0,M11.1.0"},
    {"America/Santiago", "<-04>4<-03>,M9.1.6/24,M4.1.6/24"},
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Asia/Kolkata", "IST-5:30"},
    {"Asia/Tokyo", "JST-9"},
    {"Asia/Shanghai", "CST-8"},
    {"Asia/Hong_Kong", "HKT-8"},
    {"Asia/Singapore", "<+08>-8"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
};

void useTz(const char* posix) {
  static std::string current;
  if (current != posix) {
    setenv("TZ", posix, 1);
    tzset();
    current = posix;
  }
}

int64_t civilToEpochSec(int y, int m, int d, int hh, int mm, int ss) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int64_t days = era * 146097 + static_cast<int64_t>(doe) - 719468;
  return days * 86400 + hh * 3600 + mm * 60 + ss;
}

struct Ymd {
  int y, m, d;
  bool operator==(const Ymd& o) const { return y == o.y && m == o.m && d == o.d; }
};

Ymd localYmd(const char* posix, int64_t tMs) {
  useTz(posix);
  time_t t = static_cast<time_t>(tMs / 1000);
  struct tm lt;
  localtime_r(&t, &lt);
  return {lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday};
}

int64_t tzOffsetMs(const char* posix, int64_t tMs) {
  useTz(posix);
  time_t t = static_cast<time_t>(tMs / 1000);
  struct tm lt;
  localtime_r(&t, &lt);
  int64_t asUtc = civilToEpochSec(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
                                  lt.tm_min, lt.tm_sec);
  return (asUtc - static_cast<int64_t>(t)) * 1000;
}
}

const char* posixTz(const std::string& iana) {
  for (const auto& e : kTzTable)
    if (iana == e.iana) return e.posix;
  return "UTC0";
}

int64_t localMidnightMs(const char* posix, int64_t nowMs) {
  const Ymd ymd = localYmd(posix, nowMs);
  const int64_t utcGuess = civilToEpochSec(ymd.y, ymd.m, ymd.d, 0, 0, 0) * 1000;
  const int64_t t1 = utcGuess - tzOffsetMs(posix, utcGuess);
  const int64_t t2 = utcGuess - tzOffsetMs(posix, t1);
  if (t2 == t1) return t1;
  return localYmd(posix, t2) == ymd ? t2 : t1;
}

int nowMinutes(const char* posix, int64_t nowMs) {
  useTz(posix);
  time_t t = static_cast<time_t>(nowMs / 1000);
  struct tm lt;
  localtime_r(&t, &lt);
  return (lt.tm_hour % 24) * 60 + lt.tm_min;
}

int toMin(const char* hhmm) {
  int h = 0, m = 0;
  sscanf(hhmm, "%d:%d", &h, &m);
  return h * 60 + m;
}

bool eligibility(const std::vector<Window>& schedule, const char* posix, int64_t nowMs) {
  if (schedule.empty()) return true;
  const int mins = nowMinutes(posix, nowMs);
  for (const auto& w : schedule)
    if (mins >= w.startMin && mins < w.endMin) return true;
  return false;
}

}
