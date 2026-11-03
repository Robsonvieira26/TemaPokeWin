#pragma once
// Editor de linha puro (sem windows.h) do console de debug. Recebe unidades UTF-16 digitadas
// e monta linhas UTF-8. A camada Win32 usa lastEdit()/lastText() para ecoar na tela.
#include <string>

class ConsoleLineEditor {
 public:
  enum class Edit { None, Added, Erased, Submitted };
  static constexpr size_t kMaxBytes = 255;  // alem disso, caracteres sao descartados

  // true so quando uma linha (nao vazia apos aparar espacos) foi completada por Enter
  // (\r ou \n); ela vai em *lineOut (UTF-8) e o editor zera. "\r\n" nao gera 2 linhas, pois
  // o \n cai numa linha vazia. Controles sao ignorados; par substituto vira 1 ponto de codigo.
  bool feed(wchar_t c, std::string* lineOut);

  // O que o ultimo feed() fez, para eco: Added (lastText = UTF-8 acrescentado), Erased
  // (lastText = UTF-8 do ponto de codigo removido), Submitted, ou None (nada visivel mudou).
  Edit lastEdit() const { return edit_; }
  const std::string& lastText() const { return text_; }
  const std::string& current() const { return line_; }

 private:
  std::string line_, text_;
  Edit edit_ = Edit::None;
  wchar_t high_ = 0;  // metade alta de um par substituto pendente
};
