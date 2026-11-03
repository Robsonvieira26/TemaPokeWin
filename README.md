# TamaPoke para Windows

**Este projeto agora é o desktop pet em Godot.** O jogo vive em `godot/`
(Charmander autônomo num jardim top-down, tempo real com simulação offline,
dia/noite, carinho, shell Windows com bandeja). Veja `godot/README` e o plano
em `docs/superpowers/plans/2026-11-02-godot-sim-mvp.md`; o checklist de
verificação está em `VERIFICACAO-WINDOWS.md`.

O port nativo C++/SDL (firmware ESP32 + HAL + shell do desktop) está
**arquivado** na pasta `OLD/` — todo o conteúdo abaixo descreve ele:

---

# Port nativo C++/SDL (ARQUIVADO em `OLD/`)

Port nativo para Windows, no formato *desktop pet*, do firmware ESP32 do
[TamaPoke](https://github.com/socquique/TamaPoke): um bichinho virtual que vive numa
janela redonda, sem borda, sempre no topo da tela. O firmware roda sem modificacoes
(mantido como submodulo em `OLD/upstream/`); este projeto fornece a camada de hardware
emulada (HAL) e o shell do desktop (janela, bandeja do sistema, console de debug).

Upstream fixado em: `1fff020c158282a384cc9b9357ada111e3eddba8`

## Compilar

Requisitos: Windows, Visual Studio 2022 (ou Build Tools) com C++, CMake 3.26+ e Python 3.
O CMake baixa SDL3 e nlohmann/json sozinho (precisa de internet na primeira configuracao).

```bash
git submodule update --init --recursive
cmake -B OLD/build -S OLD -G "Visual Studio 17 2022" -A x64

# Debug
cmake --build OLD/build --config Debug
# Release (CRT estatico: nao precisa do redistribuivel do VC++)
cmake --build OLD/build --config Release
```

O executavel fica em `OLD/build/<Config>/tamapoke.exe`, junto de `SDL3.dll` e da pasta
`sdcard/` (sprites). Para distribuir, use `OLD/tools/package.ps1` (veja "Empacotamento e distribuicao").

Testes:

```bash
ctest --test-dir build -C Debug --output-on-failure     # ou -C Release
```

## Executar

```bash
OLD\build\Release\tamapoke.exe
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

## Empacotamento e distribuicao

Depois de compilar em Release:

```powershell
powershell -File tools/package.ps1                 # zip sem sprites -> dist/TamaPoke-<versao>-win64.zip
powershell -File tools/package.ps1 -WithSprites    # zip pessoal    -> dist/TamaPoke-<versao>-win64-sprites.zip
```

Opcoes: `-Config Release`, `-BuildDir build`, `-OutDir dist`. A versao vem de
`project(... VERSION ...)` no `CMakeLists.txt`. Cada zip tem uma unica pasta raiz
`TamaPoke-<versao>-win64/` com `tamapoke.exe`, `SDL3.dll`, `LICENSE`, `CREDITS.md` e
`README.md`; o zip com sprites traz tambem `sdcard/mons/` (303 arquivos). Para instalar
sem o script: `cmake --install build --config Release --prefix <pasta> --component app`
(e `--component sprites` para os sprites).

### Sprites (politica)

Os sprites dos Pokemon sao CC BY-NC 4.0, **apenas uso pessoal e nao comercial**, e o
upstream recomenda distribuir so o codigo e deixar cada usuario obter os sprites
(veja [CREDITS.md](CREDITS.md)). Por isso o pacote publico (o que o CI gera) **nao
inclui sprites**; o `-WithSprites` serve para uso pessoal e nao deve ser publicado.
Sem sprites o programa abre normalmente e usa os sprites embutidos do firmware.

Para adicionar os sprites: coloque a pasta `mons\` dentro de `sdcard\` ao lado do
`tamapoke.exe` (ou aponte outra pasta com `--sd-root <pasta>`). Os arquivos (`pNNN.bin`,
`psNNN.bin` e `thumbs.bin`) sao gerados pelas ferramentas do upstream, a partir do
PMD SpriteCollab: no repositorio do TamaPoke, `python3 tools/pack_pmd.py` e
`python3 OLD/tools/make_thumbs.py` produzem `OLD/upstream/tools/sdcard/mons/` (veja a secao "Generate and
load the sprites yourself" de `OLD/upstream/README.md`). Neste repositorio essa pasta ja vem
no submodulo `OLD/upstream/tools/sdcard/mons`, e o build copia para `OLD/build/<Config>/sdcard/mons`.

### Instalador (Inno Setup)

`OLD/installer/tamapoke.iss` (Inno Setup 6) e compilado so no CI de tags `v*` ou onde o Inno
Setup estiver instalado (o instalador nao e construido nesta maquina de desenvolvimento):

```powershell
iscc /DAppVersion=0.1.0 /DSourceDir=<pasta de staging impressa pelo package.ps1> installer\tamapoke.iss
```

Comportamento: instalacao por usuario, sem administrador (padrao em
`%LOCALAPPDATA%\Programs\TamaPoke`); atalho no Menu Iniciar e, opcionalmente, na area de
trabalho; a tarefa "Iniciar o TamaPoke com o Windows" (marcada por padrao) grava o valor
`TamaPoke` em `Run`, e, se desmarcada, cria `settings.json` com `autostart: false` (se
ainda nao existir) para o app nao reativar a inicializacao; antes de instalar,
atualizar ou desinstalar, o instalador chama `tamapoke.exe --quit` para fechar o
programa que estiver rodando. Ao desinstalar, remove o valor de inicializacao e **mantem
o seu bicho e configuracoes** em `%APPDATA%\TamaPoke` a menos que voce responda "Sim" a
pergunta (desinstalacao silenciosa sempre mantem). O instalador publicado pelo CI tambem
nao inclui sprites; o CI so gera artefatos, nunca publica uma Release.

## Estrutura

- `OLD/upstream/`: firmware TamaPoke (submódulo, intocado).
- `third_party/Arduino_GFX/`: biblioteca grafica real usada pelo firmware (submodulo).
- `OLD/hal/`: emulação do hardware/Arduino (tela, toque, RTC, SD, áudio, preferências).
- `OLD/src/`: shell do desktop pet; `OLD/src/win/` tem bandeja, autostart, instância única e console.
- `OLD/assets/`: ícone, recursos do Windows (`app.rc`) e a fonte do ícone.
- `OLD/tools/`: geradores (`gen_protos.py`, `extract_u8g2_fonts.py`) e `package.ps1`; `tools/` na raiz agora só tem o `make_icon.py` (usado pelo Godot).
- `OLD/installer/`: script do instalador Inno Setup.
- `OLD/tests/`: testes (micro-framework próprio em C++ e `unittest` para os scripts Python).
- `docs/`: decisoes, planos e especificacoes.

## Licencas

Codigo deste repositorio: MIT ([LICENSE](LICENSE)). Sprites, firmware e bibliotecas
tem licencas proprias, e os sprites sao **apenas para uso nao comercial**: veja
[CREDITS.md](CREDITS.md). Pokemon e marca de seus donos; este projeto nao e afiliado
a eles.
