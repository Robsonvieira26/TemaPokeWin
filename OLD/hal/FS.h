#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

// Apenas o que o upstream usa (sdmon.cpp): open/size/read/write/close/name/openNextFile.
#define FILE_READ   "r"
#define FILE_WRITE  "w"
#define FILE_APPEND "a"

// Copia barata (shared_ptr): o handle fecha quando a ultima copia morre ou em close().
class File {
 public:
  File() = default;

  explicit operator bool() const;
  size_t size() const;
  size_t read(uint8_t* buf, size_t len);
  size_t write(const uint8_t* buf, size_t len);
  void close();
  const char* name() const;   // so o nome base, como no ESP32
  File openNextFile();        // File invalido ao fim da listagem

 private:
  friend class SDMMCClass;
  struct Impl;
  std::shared_ptr<Impl> p_;
};
