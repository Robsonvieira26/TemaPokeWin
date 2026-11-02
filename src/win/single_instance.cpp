#include "single_instance.h"
#include <windows.h>
#include <string>

namespace {
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

bool SingleInstance::acquire(const char* baseName) {
  if (mutex_) return true;  // ja somos a instancia primaria
  std::wstring base;
  if (!toWide(baseName, base) || base.empty()) return false;

  HANDLE mutex = CreateMutexW(nullptr, FALSE, (L"Local\\" + base).c_str());
  if (!mutex) return false;
  const bool existed = GetLastError() == ERROR_ALREADY_EXISTS;

  // Evento auto-reset, inicialmente nao sinalizado; criado/aberto por ambos os lados.
  HANDLE event = CreateEventW(nullptr, FALSE, FALSE, (L"Local\\" + base + L"Show").c_str());
  if (!event) {
    CloseHandle(mutex);
    return false;
  }

  if (existed) {
    SetEvent(event);  // pede para a primaria aparecer
    CloseHandle(event);
    CloseHandle(mutex);
    return false;
  }
  mutex_ = mutex;
  event_ = event;
  return true;
}

bool SingleInstance::showRequested() {
  if (!event_) return false;
  return WaitForSingleObject((HANDLE)event_, 0) == WAIT_OBJECT_0;
}

void SingleInstance::release() {
  if (event_) { CloseHandle((HANDLE)event_); event_ = nullptr; }
  if (mutex_) { CloseHandle((HANDLE)mutex_); mutex_ = nullptr; }
}
