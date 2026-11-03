#pragma once

// Configuracoes persistentes do app (settings.json em halDataDir()).
struct Settings {
  float scale = 1.0f;         // somente 0.5, 1.0 ou 2.0 (outros valores viram 1.0)
  bool alwaysOnTop = true;
  bool autostart = true;
  bool soundEnabled = true;
  // Posicao da janela. Valida somente se hasPosition == true; entao windowX/windowY sao
  // coordenadas reais da area virtual e PODEM ser negativas (monitor a esquerda/acima).
  // hasPosition == false significa "centralizar" (windowX/Y ficam em -1 e nao sao gravados).
  // Validar contra os monitores atuais e responsabilidade do shell, nao daqui.
  bool hasPosition = false;
  int windowX = -1;
  int windowY = -1;
  bool startHidden = false;   // definido por --hidden; nunca persistido
};

// Nunca lanca: arquivo ausente/corrompido/de tipo errado resulta em defaults (por campo).
Settings settingsLoad();
// Grava de forma atomica (settings.json.tmp + MoveFileEx). Falhas sao silenciosas.
void settingsSave(const Settings& s);
