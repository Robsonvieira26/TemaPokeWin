#include "autostart.h"
#include <windows.h>
#include <string>

namespace {
const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool toWide(const char* s, std::wstring& out) {
  if (!s) return false;
  int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
  if (n <= 0) return false;
  out.assign((size_t)n - 1, L'\0');
  if (n > 1 && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, &out[0], n) <= 0)
    return false;
  return true;
}
}  // namespace

bool autostartEnable(const char* valueName, const char* commandLine) {
  std::wstring name, cmd;
  if (!toWide(valueName, name) || !toWide(commandLine, cmd) || name.empty()) return false;
  HKEY key{};
  if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr,
                      &key, nullptr) != ERROR_SUCCESS)
    return false;
  // Tamanho em BYTES, incluindo o NUL terminador.
  LSTATUS st = RegSetValueExW(key, name.c_str(), 0, REG_SZ, (const BYTE*)cmd.c_str(),
                              (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
  return st == ERROR_SUCCESS;
}

bool autostartDisable(const char* valueName) {
  std::wstring name;
  if (!toWide(valueName, name) || name.empty()) return false;
  HKEY key{};
  LSTATUS st = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key);
  if (st == ERROR_FILE_NOT_FOUND) return true;  // chave ausente = ja desabilitado
  if (st != ERROR_SUCCESS) return false;
  st = RegDeleteValueW(key, name.c_str());
  RegCloseKey(key);
  return st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND;
}

bool autostartEnabled(const char* valueName) {
  std::wstring name;
  if (!toWide(valueName, name) || name.empty()) return false;
  HKEY key{};
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
    return false;
  DWORD type = 0, size = 0;
  LSTATUS st = RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &size);
  RegCloseKey(key);
  return st == ERROR_SUCCESS && type == REG_SZ;
}
