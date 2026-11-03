#include "settings.h"
#include "hal_control.h"
#include <nlohmann/json.hpp>
#include <climits>
#include <filesystem>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

namespace {
std::string path() { return std::string(halDataDir()) + "/settings.json"; }

// Somente 0.5, 1.0 e 2.0 sao validos; qualquer outro valor vira 1.0.
float escalaValida(double s) {
  if (s > 0.4 && s < 0.6) return 0.5f;
  if (s > 1.9 && s < 2.1) return 2.0f;
  return 1.0f;
}

// Leitura tolerante: campo ausente ou de tipo errado mantem o valor atual.
void lerBool(const json& j, const char* k, bool& out) {
  auto it = j.find(k);
  if (it != j.end() && it->is_boolean()) out = it->get<bool>();
}

bool lerInt(const json& j, const char* k, int& out) {
  auto it = j.find(k);
  if (it == j.end() || !it->is_number_integer()) return false;
  long long v = it->get<long long>();
  if (v < INT_MIN || v > INT_MAX) return false;
  out = (int)v;
  return true;
}
}  // namespace

Settings settingsLoad() {
  Settings s;
  try {
    std::ifstream in(path(), std::ios::binary);
    if (!in) return s;
    json j = json::parse(in);
    if (!j.is_object()) return Settings{};
    auto sc = j.find("scale");
    if (sc != j.end() && sc->is_number()) s.scale = escalaValida(sc->get<double>());
    lerBool(j, "alwaysOnTop", s.alwaysOnTop);
    lerBool(j, "autostart", s.autostart);
    lerBool(j, "soundEnabled", s.soundEnabled);
    int x, y;
    if (lerInt(j, "windowX", x) && lerInt(j, "windowY", y)) {
      s.hasPosition = true;
      s.windowX = x;
      s.windowY = y;
    }
  } catch (...) {
    return Settings{};
  }
  return s;
}

void settingsSave(const Settings& s) {
  try {
    std::error_code ec;
    std::filesystem::create_directories(halDataDir(), ec);
    json j{{"scale", escalaValida(s.scale)},
           {"alwaysOnTop", s.alwaysOnTop},
           {"autostart", s.autostart},
           {"soundEnabled", s.soundEnabled}};
    if (s.hasPosition) {
      j["windowX"] = s.windowX;
      j["windowY"] = s.windowY;
    }
    std::string p = path(), tmp = p + ".tmp";
    bool ok;
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      out << j.dump(2);
      ok = out.good();
    }
    if (!ok) return;
#ifdef _WIN32
    MoveFileExA(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING);
#else
    std::filesystem::rename(tmp, p, ec);
#endif
  } catch (...) {
    // salvar e best-effort: nunca derruba o app
  }
}
