# TamaPoke para Windows

Port nativo para Windows, no formato *desktop pet*, do firmware ESP32 do
[TamaPoke](https://github.com/socquique/TamaPoke): um bichinho virtual que vive numa
janela redonda, sem borda, sempre no topo da tela. O firmware roda sem modificacoes
(mantido como submodulo em `upstream/`); este projeto fornece a camada de hardware
emulada (HAL) e o shell do desktop (janela, bandeja do sistema, console de debug).

Upstream fixado em: `1fff020c158282a384cc9b9357ada111e3eddba8`

## Compilar

Requisitos: Windows, Visual Studio 2022 (ou Build Tools) com C++, CMake 3.26+ e Python 3.
O CMake baixa SDL3 e nlohmann/json sozinho (precisa de internet na primeira configuracao).

```bash
git submodule update --init --recursive
cmake -B build -G "Visual Studio 17 2022" -A x64

# Debug
cmake --build build --config Debug
# Release (CRT estatico: nao precisa do redistribuivel do VC++)
cmake --build build --config Release
```

O executavel fica em `build/<Config>/tamapoke.exe`, junto de `SDL3.dll` e da pasta
`sdcard/` (sprites). Copie essas tres coisas juntas para distribuir.

Testes:

```bash
ctest --test-dir build -C Debug --output-on-failure     # ou -C Release
```

## Executar

```bash
build\Release\tamapoke.exe
```

Opcoes de linha de comando:

| Opcao | Efeito |
|---|---|
| `--hidden` | inicia escondido na bandeja (a janela nao aparece) |
| `--no-autostart` | nao toca no registro de inicializacao com o Windows |
| `--data-dir <pasta>` | pasta dos dados (save, configuracoes, log) |
| `--sd-root <pasta>` | pasta do "cartao SD" com os sprites (padrao: `sdcard` ao lado do exe) |
| `--instance <nome>` | nome da instancia unica (uma segunda execucao so avisa a primeira) |
| `--quit` | pede a instancia em execucao para salvar e sair, espera ela fechar (ate 10 s; codigo 2 se nao fechar) e termina; sem instancia, nao faz nada (codigo 0). Usado por instalador/desinstalador |
| `--console` | abre o console de debug ao iniciar |

## Controles

- Clique esquerdo: toque na tela do bicho.
- Arrastar com o botao direito: move a janela.
- Clique direito (sem arrastar): menu (o mesmo da bandeja).
- Setas: gestos de deslizar (esquerda, direita, cima, baixo).
- F2: botao de energia (apaga/acende a tela).
- F12: abre/fecha o console de debug.
- Esc: nao faz nada.
- Fechar a janela (X ou Alt+F4) so esconde o bicho na bandeja.
- Bandeja: menu com Mostrar/Esconder, Som, Sempre no topo, Escala (0,5x, 1x, 2x),
  Iniciar com o Windows e **Sair** (e o unico jeito de encerrar o programa).

## Onde ficam os dados

Em `%APPDATA%\TamaPoke\` (ou na pasta de `--data-dir`):

- `nvs.json`: o save do bicho;
- `settings.json`: configuracoes do shell (som, escala, posicao, sempre no topo, autostart);
- `log.txt`: saida serial do firmware.

## Iniciar com o Windows

Por padrao o programa se registra em
`HKCU\Software\Microsoft\Windows\CurrentVersion\Run` (valor `TamaPoke`) a cada
inicializacao. Desative em "Iniciar com o Windows" no menu da bandeja, que remove o
valor. `--no-autostart` faz o programa nao mexer nesse registro naquela execucao
(util em testes).

## Estrutura

- `upstream/`: firmware TamaPoke (submodulo, intocado).
- `third_party/Arduino_GFX/`: biblioteca grafica real usada pelo firmware (submodulo).
- `hal/`: emulacao do hardware/Arduino (tela, toque, RTC, SD, audio, preferencias).
- `src/`: shell do desktop pet; `src/win/` tem bandeja, autostart, instancia unica e console.
- `assets/`: icone, recursos do Windows (`app.rc`) e a fonte do icone.
- `tools/`: geradores (`make_icon.py`, `gen_protos.py`, `extract_u8g2_fonts.py`).
- `tests/`: testes (micro-framework proprio em C++ e `unittest` para os scripts Python).
- `docs/`: decisoes, planos e especificacoes.

## Licencas

Codigo deste repositorio: MIT ([LICENSE](LICENSE)). Sprites, firmware e bibliotecas
tem licencas proprias, e os sprites sao **apenas para uso nao comercial**: veja
[CREDITS.md](CREDITS.md). Pokemon e marca de seus donos; este projeto nao e afiliado
a eles.
