#include "Preferences.h"
#include "hal_control.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

namespace {
json gDoc = json::object();
bool gLoaded = false;
std::string gLoadedFrom;

std::string nvsPath() { return std::string(halDataDir()) + "/nvs.json"; }

const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string b64encode(const uint8_t* d, size_t n) {
  std::string o;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = (uint32_t)d[i] << 16;
    if (i + 1 < n) v |= (uint32_t)d[i + 1] << 8;
    if (i + 2 < n) v |= d[i + 2];
    o += kB64[(v >> 18) & 63];
    o += kB64[(v >> 12) & 63];
    o += (i + 1 < n) ? kB64[(v >> 6) & 63] : '=';
    o += (i + 2 < n) ? kB64[v & 63] : '=';
  }
  return o;
}

std::vector<uint8_t> b64decode(const std::string& s) {
  std::vector<uint8_t> o;
  uint32_t acc = 0;
  int bits = 0;
  for (char c : s) {
    if (c == '\0') break;
    const char* p = std::strchr(kB64, c);
    if (!p) continue;  // '=' e lixo
    acc = (acc << 6) | (uint32_t)(p - kB64);
    bits += 6;
    if (bits >= 8) { bits -= 8; o.push_back((uint8_t)((acc >> bits) & 0xFF)); }
  }
  return o;
}

void loadOnce() {
  std::string path = nvsPath();
  if (gLoaded && gLoadedFrom == path) return;
  gDoc = json::object();
  gLoaded = true;
  gLoadedFrom = path;
  std::ifstream in(path, std::ios::binary);
  if (!in) return;
  try {
    json parsed = json::parse(in);
    if (!parsed.is_object()) throw std::runtime_error("raiz nao e objeto");
    gDoc = std::move(parsed);
  } catch (...) {
    in.close();
    std::error_code ec;
    std::filesystem::rename(path, path + ".bad-" + std::to_string((long long)std::time(nullptr)), ec);
    gDoc = json::object();
  }
}

void saveAtomic() {
  std::error_code ec;
  std::filesystem::create_directories(halDataDir(), ec);
  std::string path = nvsPath(), tmp = path + ".tmp";
  std::string text = gDoc.dump(2, ' ', false, json::error_handler_t::replace);
  bool ok;
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    out << text;
    ok = out.good();
  }
  if (!ok) return;
#ifdef _WIN32
  MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
#else
  std::filesystem::rename(tmp, path, ec);
#endif
}

// Cada chave e {"t":"<tipo>","v":<valor>}: ler com o getter errado devolve o default (NVS).
json* findEntry(const std::string& ns, const char* key) {
  auto n = gDoc.find(ns);
  if (n == gDoc.end() || !n->is_object()) return nullptr;
  auto k = n->find(key);
  if (k == n->end() || !k->is_object()) return nullptr;
  return &(*k);
}

template <typename T>
T getTyped(const std::string& ns, bool open, const char* key, const char* type, T def) {
  if (!open) return def;
  json* e = findEntry(ns, key);
  if (!e || !e->contains("t") || !(*e)["t"].is_string() || (*e)["t"] != type || !e->contains("v")) return def;
  try { return (*e)["v"].get<T>(); } catch (...) { return def; }
}

template <typename V>
size_t putEntry(const std::string& ns, bool open, bool ro, const char* key, const char* type, V v, size_t ret) {
  if (!open || ro) return 0;
  if (!gDoc[ns].is_object()) gDoc[ns] = json::object();
  gDoc[ns][key] = json{{"t", type}, {"v", v}};
  saveAtomic();
  return ret;
}
}  // namespace

bool Preferences::begin(const char* name, bool readOnly) {
  ns_ = name ? name : "";
  ro_ = readOnly;
  open_ = true;
  loadOnce();
  return true;
}
void Preferences::end() { open_ = false; }

bool Preferences::clear() {
  if (!open_ || ro_) return false;
  gDoc[ns_] = json::object();
  saveAtomic();
  return true;
}
bool Preferences::isKey(const char* key) { return open_ && findEntry(ns_, key) != nullptr; }
bool Preferences::remove(const char* key) {
  if (!open_ || ro_ || !findEntry(ns_, key)) return false;
  gDoc[ns_].erase(key);
  saveAtomic();
  return true;
}

size_t Preferences::putBool(const char* k, bool v)       { return putEntry(ns_, open_, ro_, k, "b",   v, 1); }
size_t Preferences::putUChar(const char* k, uint8_t v)   { return putEntry(ns_, open_, ro_, k, "u8",  v, 1); }
size_t Preferences::putChar(const char* k, int8_t v)     { return putEntry(ns_, open_, ro_, k, "i8",  v, 1); }
size_t Preferences::putShort(const char* k, int16_t v)   { return putEntry(ns_, open_, ro_, k, "i16", v, 2); }
size_t Preferences::putUShort(const char* k, uint16_t v) { return putEntry(ns_, open_, ro_, k, "u16", v, 2); }
size_t Preferences::putUInt(const char* k, uint32_t v)   { return putEntry(ns_, open_, ro_, k, "u32", v, 4); }
size_t Preferences::putString(const char* k, const char* v) {
  std::string s = v ? v : "";
  return putEntry(ns_, open_, ro_, k, "s", s, s.size());
}
size_t Preferences::putBytes(const char* k, const void* v, size_t len) {
  return putEntry(ns_, open_, ro_, k, "blob", b64encode((const uint8_t*)v, len), len);
}

bool Preferences::getBool(const char* k, bool d)           { return getTyped(ns_, open_, k, "b",   d); }
uint8_t Preferences::getUChar(const char* k, uint8_t d)    { return getTyped(ns_, open_, k, "u8",  d); }
int8_t Preferences::getChar(const char* k, int8_t d)       { return getTyped(ns_, open_, k, "i8",  d); }
int16_t Preferences::getShort(const char* k, int16_t d)    { return getTyped(ns_, open_, k, "i16", d); }
uint16_t Preferences::getUShort(const char* k, uint16_t d) { return getTyped(ns_, open_, k, "u16", d); }
uint32_t Preferences::getUInt(const char* k, uint32_t d)   { return getTyped(ns_, open_, k, "u32", d); }

size_t Preferences::getString(const char* k, char* buf, size_t maxLen) {
  if (!buf || maxLen == 0) return 0;
  std::string s = getTyped<std::string>(ns_, open_, k, "s", std::string());
  size_t n = s.size() < maxLen - 1 ? s.size() : maxLen - 1;
  std::memcpy(buf, s.data(), n);
  buf[n] = '\0';
  return n;
}
size_t Preferences::getBytes(const char* k, void* buf, size_t maxLen) {
  if (!open_ || !buf) return 0;
  std::vector<uint8_t> d = b64decode(getTyped<std::string>(ns_, open_, k, "blob", std::string()));
  size_t n = d.size() < maxLen ? d.size() : maxLen;
  if (n) std::memcpy(buf, d.data(), n);  // ausente => 0 e buffer intacto, como a NVS
  return n;
}
