#include "win/console.h"
#include <windows.h>
#include <cstdio>
#include "win/console_line.h"

namespace {
bool g_open = false;
HANDLE g_in = INVALID_HANDLE_VALUE, g_out = INVALID_HANDLE_VALUE;
ConsoleLineEditor g_edit;
void (*g_closeCb)() = nullptr;

const char* kBanner =
    "TamaPoke - console de debug (F12 fecha). Comandos do firmware:\r\n"
    "  STATS HEALTH REG TIME [epoch] RTCSET <epoch> LVL <n> SPEC <n> NICK <nome>\r\n"
    "  HATCH SHINY EGGS CAREDAY BYE RUN ABANDON BEEP GAL WIPE SDINFO LS\r\n"
    "Fechar a janela e Ctrl+C estao desativados; use F12 ou o menu da bandeja.\r\n";

void writeW(const wchar_t* w, int n) {
  DWORD done = 0;
  if (g_out != INVALID_HANDLE_VALUE && n > 0) WriteConsoleW(g_out, w, (DWORD)n, &done, nullptr);
}
void writeUtf8(const std::string& s) {
  if (s.empty()) return;
  wchar_t w[1024];
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w, 1024);
  writeW(w, n);
}
void writeAscii(const char* s) { writeUtf8(s); }

BOOL WINAPI ctrlHandler(DWORD type) {
  switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
      return TRUE;  // ignorados: nao matam o pet
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      if (g_closeCb) g_closeCb();
      for (int i = 0; i < 80; ++i) Sleep(50);  // da tempo ao laco principal de salvar e sair
      return TRUE;
    default:
      return FALSE;
  }
}

void redirectStdio(bool toConsole) {
  FILE* f = nullptr;
  freopen_s(&f, toConsole ? "CONOUT$" : "NUL", "w", stdout);
  freopen_s(&f, toConsole ? "CONOUT$" : "NUL", "w", stderr);
  freopen_s(&f, toConsole ? "CONIN$" : "NUL", "r", stdin);
}

bool open() {
  if (!AllocConsole()) return false;
  HWND hw = GetConsoleWindow();
  if (hw) {
    HMENU m = GetSystemMenu(hw, FALSE);
    if (m) {
      RemoveMenu(m, SC_CLOSE, MF_BYCOMMAND);
      DrawMenuBar(hw);
    }
  }
  SetConsoleCtrlHandler(ctrlHandler, TRUE);
  SetConsoleTitleW(L"TamaPoke - console de debug");
  SetConsoleOutputCP(CP_UTF8);
  redirectStdio(true);
  g_out = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                      nullptr, OPEN_EXISTING, 0, nullptr);
  g_in = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     nullptr, OPEN_EXISTING, 0, nullptr);
  if (g_in != INVALID_HANDLE_VALUE) {
    DWORD mode = 0;
    if (GetConsoleMode(g_in, &mode)) {
      mode &= ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_QUICK_EDIT_MODE);
      mode |= ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS;
      SetConsoleMode(g_in, mode);
    }
  }
  g_edit = ConsoleLineEditor();
  g_open = true;
  writeAscii(kBanner);
  writeAscii("> ");
  return true;
}
}  // namespace

void consoleSetCloseCallback(void (*cb)()) { g_closeCb = cb; }
bool consoleIsOpen() { return g_open; }

void consoleClose() {
  if (!g_open) return;
  g_open = false;
  if (g_in != INVALID_HANDLE_VALUE) CloseHandle(g_in);
  if (g_out != INVALID_HANDLE_VALUE) CloseHandle(g_out);
  g_in = g_out = INVALID_HANDLE_VALUE;
  redirectStdio(false);
  FreeConsole();
  SetConsoleCtrlHandler(ctrlHandler, FALSE);
}

bool consoleToggle() {
  if (g_open) {
    consoleClose();
    return false;
  }
  if (!open()) return false;
  return true;
}

bool consoleReadLine(std::string* out) {
  if (!g_open || g_in == INVALID_HANDLE_VALUE) return false;
  DWORD n = 0;
  while (GetNumberOfConsoleInputEvents(g_in, &n) && n > 0) {
    INPUT_RECORD rec[16];
    DWORD got = 0;
    if (!ReadConsoleInputW(g_in, rec, 16, &got)) return false;
    for (DWORD i = 0; i < got; ++i) {
      if (rec[i].EventType != KEY_EVENT || !rec[i].Event.KeyEvent.bKeyDown) continue;
      const wchar_t c = rec[i].Event.KeyEvent.uChar.UnicodeChar;
      if (c == 0) continue;
      // Eventos restantes deste lote ficam perdidos se retornarmos cedo; por isso so retornamos
      // ao fim do lote (uma linha por chamada basta: o shell chama a cada iteracao).
      std::string line;
      const bool done = g_edit.feed(c, &line);
      switch (g_edit.lastEdit()) {
        case ConsoleLineEditor::Edit::Added: writeUtf8(g_edit.lastText()); break;
        case ConsoleLineEditor::Edit::Erased: writeAscii("\b \b"); break;
        case ConsoleLineEditor::Edit::Submitted: writeAscii("\r\n> "); break;
        default: break;
      }
      if (done) {
        // devolve a linha; eventos ainda nao lidos neste lote ficam para a proxima chamada
        if (i + 1 < got) {
          // reinjeta os restantes para nao perde-los
          DWORD w = 0;
          WriteConsoleInputW(g_in, rec + i + 1, got - (i + 1), &w);
        }
        *out = line;
        return true;
      }
    }
  }
  return false;
}
