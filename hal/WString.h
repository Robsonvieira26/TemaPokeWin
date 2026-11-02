#pragma once
#include <string>
#include <cstdlib>

// String do Arduino sobre std::string. So os metodos que o upstream usa
// (grep da Task 5): trim, length, startsWith, substring(1/2 args), toInt,
// c_str, indexOf(const char*), lastIndexOf(char).
class String {
 public:
  String() = default;
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}

  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }

  void trim() {
    size_t b = s_.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) { s_.clear(); return; }
    size_t e = s_.find_last_not_of(" \t\r\n");
    s_ = s_.substr(b, e - b + 1);
  }
  bool startsWith(const char* p) const { return s_.rfind(p, 0) == 0; }
  String substring(size_t from) const {
    return from >= s_.size() ? String() : String(s_.substr(from));
  }
  String substring(size_t from, size_t to) const {
    if (from >= s_.size() || to <= from) return String();
    if (to > s_.size()) to = s_.size();
    return String(s_.substr(from, to - from));
  }
  int indexOf(const char* n) const {
    size_t p = s_.find(n); return p == std::string::npos ? -1 : (int)p;
  }
  int lastIndexOf(char c) const {
    size_t p = s_.rfind(c); return p == std::string::npos ? -1 : (int)p;
  }
  long toInt() const { return std::strtol(s_.c_str(), nullptr, 10); }

 private:
  std::string s_;
};
