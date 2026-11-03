#include "win/console_line.h"

namespace {
void appendUtf8(std::string* s, unsigned cp) {
  if (cp < 0x80) {
    s->push_back((char)cp);
  } else if (cp < 0x800) {
    s->push_back((char)(0xC0 | (cp >> 6)));
    s->push_back((char)(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    s->push_back((char)(0xE0 | (cp >> 12)));
    s->push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
    s->push_back((char)(0x80 | (cp & 0x3F)));
  } else {
    s->push_back((char)(0xF0 | (cp >> 18)));
    s->push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
    s->push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
    s->push_back((char)(0x80 | (cp & 0x3F)));
  }
}
}  // namespace

bool ConsoleLineEditor::feed(wchar_t c, std::string* lineOut) {
  edit_ = Edit::None;
  text_.clear();
  const unsigned u = (unsigned)(unsigned short)c;
  unsigned cp = 0;

  if (u >= 0xDC00 && u <= 0xDFFF) {  // metade baixa
    if (!high_) return false;
    cp = 0x10000 + (((unsigned)high_ - 0xD800) << 10) + (u - 0xDC00);
    high_ = 0;
  } else {
    high_ = 0;  // alto pendente seguido de outra coisa: descartado
    if (u >= 0xD800 && u <= 0xDBFF) {
      high_ = c;
      return false;
    }
    if (u == '\r' || u == '\n') {
      size_t b = line_.find_first_not_of(" \t");
      size_t e = line_.find_last_not_of(" \t");
      std::string out = b == std::string::npos ? std::string() : line_.substr(b, e - b + 1);
      line_.clear();
      if (out.empty()) return false;
      if (lineOut) *lineOut = out;
      edit_ = Edit::Submitted;
      return true;
    }
    if (u == 0x08 || u == 0x7F) {  // backspace: remove o ultimo ponto de codigo
      if (line_.empty()) return false;
      size_t i = line_.size() - 1;
      while (i > 0 && ((unsigned char)line_[i] & 0xC0) == 0x80) --i;
      text_ = line_.substr(i);
      line_.erase(i);
      edit_ = Edit::Erased;
      return false;
    }
    if (u < 0x20) return false;  // demais controles
    cp = u;
  }
  std::string enc;
  appendUtf8(&enc, cp);
  if (line_.size() + enc.size() > kMaxBytes) return false;
  line_ += enc;
  text_ = enc;
  edit_ = Edit::Added;
  return false;
}
