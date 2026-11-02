#pragma once
#include "FS.h"
#include "WString.h"

// Assinaturas identicas as chamadas de upstream/sdmon.cpp.
class SDMMCClass {
 public:
  void setPins(int clk, int cmd, int d0) { (void)clk; (void)cmd; (void)d0; }
  bool begin(const char* mountpoint = "/sdcard", bool mode1bit = false,
             bool formatIfMountFailed = false);
  File open(const char* path, const char* mode = FILE_READ);
  bool exists(const char* path);
  bool mkdir(const char* path);
  bool remove(const char* path);
  // sdmon.cpp passa String (PUT <ruta>); o FS do Arduino tem estas sobrecargas.
  File open(const String& path, const char* mode = FILE_READ) { return open(path.c_str(), mode); }
  bool exists(const String& path) { return exists(path.c_str()); }
  bool remove(const String& path) { return remove(path.c_str()); }
  uint64_t cardSize();
  uint64_t totalBytes();
  uint64_t usedBytes();
};
extern SDMMCClass SD_MMC;
