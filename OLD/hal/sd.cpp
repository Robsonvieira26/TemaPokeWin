#include "SD_MMC.h"
#include "hal_control.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

struct File::Impl {
  std::FILE* f = nullptr;
  bool isDir = false;
  size_t size = 0;
  std::string name;
  std::unique_ptr<fs::directory_iterator> it;
  ~Impl() { if (f) std::fclose(f); }
};

namespace {
// "/mons/p001.bin" -> <raiz>/mons/p001.bin. Rejeita ".." e ':' (escape da raiz / unidade).
bool resolve(const char* path, std::string& out) {
  std::string p = path ? path : "";
  for (char& c : p) if (c == '\\') c = '/';
  size_t i = 0;
  while (i <= p.size()) {
    size_t j = p.find('/', i);
    if (j == std::string::npos) j = p.size();
    std::string comp = p.substr(i, j - i);
    if (comp == ".." || comp.find(':') != std::string::npos) return false;
    i = j + 1;
  }
  while (!p.empty() && p.front() == '/') p.erase(p.begin());
  out = std::string(halSdRoot()) + "/" + p;
  return true;
}
}  // namespace

File::operator bool() const { return p_ && (p_->f || p_->isDir); }
size_t File::size() const { return p_ ? p_->size : 0; }
const char* File::name() const { return p_ ? p_->name.c_str() : ""; }

size_t File::read(uint8_t* buf, size_t len) {
  if (!p_ || !p_->f) return 0;
  return std::fread(buf, 1, len, p_->f);
}
size_t File::write(const uint8_t* buf, size_t len) {
  if (!p_ || !p_->f) return 0;
  return std::fwrite(buf, 1, len, p_->f);
}
void File::close() {
  if (!p_) return;
  if (p_->f) { std::fclose(p_->f); p_->f = nullptr; }
  p_->isDir = false;
  p_->it.reset();
}

File File::openNextFile() {
  File none;
  if (!p_ || !p_->isDir || !p_->it) return none;
  std::error_code ec;
  if (*p_->it == fs::directory_iterator()) return none;
  fs::path child = (*p_->it)->path();
  p_->it->increment(ec);
  File r;
  r.p_ = std::make_shared<Impl>();
  r.p_->name = child.filename().string();
  if (fs::is_directory(child, ec)) {
    r.p_->isDir = true;
    r.p_->it = std::make_unique<fs::directory_iterator>(child, ec);
  } else {
    r.p_->f = std::fopen(child.string().c_str(), "rb");
    if (!r.p_->f) return none;
    size_t s = (size_t)fs::file_size(child, ec);
    r.p_->size = ec ? 0 : s;
  }
  return r;
}

SDMMCClass SD_MMC;

bool SDMMCClass::begin(const char*, bool, bool) {
  std::error_code ec;
  return fs::is_directory(halSdRoot(), ec);
}

File SDMMCClass::open(const char* path, const char* mode) {
  File none;
  std::string full;
  if (!resolve(path, full)) return none;
  std::error_code ec;
  File r;
  r.p_ = std::make_shared<File::Impl>();
  r.p_->name = fs::path(full).filename().string();
  if (fs::is_directory(full, ec)) {
    r.p_->isDir = true;
    r.p_->it = std::make_unique<fs::directory_iterator>(full, ec);
    return ec ? none : r;
  }
  std::string m = std::string(mode ? mode : FILE_READ) + "b";
  r.p_->f = std::fopen(full.c_str(), m.c_str());
  if (!r.p_->f) return none;
  size_t s = (size_t)fs::file_size(full, ec);
  r.p_->size = ec ? 0 : s;
  return r;
}

bool SDMMCClass::exists(const char* path) {
  std::string full;
  std::error_code ec;
  return resolve(path, full) && fs::exists(full, ec);
}
bool SDMMCClass::mkdir(const char* path) {
  std::string full;
  if (!resolve(path, full)) return false;
  std::error_code ec;
  fs::create_directories(full, ec);
  return !ec;
}
bool SDMMCClass::remove(const char* path) {
  std::string full;
  if (!resolve(path, full)) return false;
  std::error_code ec;
  return fs::remove(full, ec);
}
uint64_t SDMMCClass::cardSize() {
  std::error_code ec;
  auto si = fs::space(halSdRoot(), ec);
  return ec ? 0 : (uint64_t)si.capacity;
}
uint64_t SDMMCClass::totalBytes() { return cardSize(); }
uint64_t SDMMCClass::usedBytes() {
  std::error_code ec;
  auto si = fs::space(halSdRoot(), ec);
  return ec ? 0 : (uint64_t)(si.capacity - si.available);
}
