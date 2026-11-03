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
  HANDLE quit = CreateEventW(nullptr, FALSE, FALSE, (L"Local\\" + base + L"Quit").c_str());
  if (!quit) {
    CloseHandle(event);
    CloseHandle(mutex);
    return false;
  }
  mutex_ = mutex;
  event_ = event;
  quitEvent_ = quit;
  return true;
}

bool SingleInstance::quitRequested() {
  if (!quitEvent_) return false;
  return WaitForSingleObject((HANDLE)quitEvent_, 0) == WAIT_OBJECT_0;
}

bool SingleInstance::requestQuit(const char* baseName) {
  std::wstring base;
  if (!toWide(baseName, base) || base.empty()) return false;
  HANDLE quit = OpenEventW(EVENT_MODIFY_STATE, FALSE, (L"Local\\" + base + L"Quit").c_str());
  if (!quit) return false;  // nada em execucao: nao cria nada
  const bool ok = SetEvent(quit) != 0;
  CloseHandle(quit);
  return ok;
}

bool SingleInstance::isRunning(const char* baseName) {
  std::wstring base;
  if (!toWide(baseName, base) || base.empty()) return false;
  HANDLE m = OpenMutexW(SYNCHRONIZE, FALSE, (L"Local\\" + base).c_str());
  if (!m) return false;
  CloseHandle(m);
  return true;
}

bool SingleInstance::waitForExit(const char* baseName, unsigned timeoutMs) {
  const ULONGLONG t0 = GetTickCount64();
  while (isRunning(baseName)) {
    if (GetTickCount64() - t0 >= timeoutMs) return false;
    Sleep(50);
  }
  return true;
}

bool SingleInstance::showRequested() {
  if (!event_) return false;
  return WaitForSingleObject((HANDLE)event_, 0) == WAIT_OBJECT_0;
}

void SingleInstance::release() {
  if (quitEvent_) { CloseHandle((HANDLE)quitEvent_); quitEvent_ = nullptr; }
  if (event_) { CloseHandle((HANDLE)event_); event_ = nullptr; }
  if (mutex_) { CloseHandle((HANDLE)mutex_); mutex_ = nullptr; }
}
