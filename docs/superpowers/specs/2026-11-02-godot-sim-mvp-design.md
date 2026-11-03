# Simulação Godot — MVP completo (spec de design)

**Data:** 2026-11-02
**Status:** aprovado (brainstorming com o Robson)
**Base:** projeto `godot/` existente (estrutura inicial, sprites PMD, IA por necessidades, HUD, auto-save — PRs #1–#6)

## 1. Objetivo

Transformar a simulação Godot em um *desktop pet* autônomo completo: um Charmander
que vive sozinho num jardim top-down na tela do Windows, em tempo real, se cuidando
sem nenhuma intervenção obrigatória — o jogador observa, dá carinho e ajusta a
janela. Um componente por vez, evoluindo o projeto existente (sem reescrita).

## 2. Decisões tomadas

| Questão | Decisão |
|---|---|
| Experiência | Desktop pet autônomo. O bicho resolve as próprias necessidades; não há morte, desmaio ou derrota. O "jogo" é observar e a progressão de XP/nível. |
| Tempo | Relógio do PC. Tempo real dentro da sessão; progressão continua com o app fechado (simulação fiel: IA em avanço rápido). |
| Conteúdo | Só a linha Charmander (nível 1 → 17 Charmeleon → 37 Charizard). Primeira execução começa direto no Charmander (sem ovo). |
| Mundo | Top-down, mapa 1088×896 (jardim outonal, `assets/garden-map.png`). Janela 480×480 sem borda; câmera com limites segue o bicho. |
| Locomoção | Jardim inteiro navegável: polígonos autorados à mão + `NavigationAgent2D`; pontes conectam as duas margens do rio. |
| Zonas | Dojo com alvo (inferior direito) = treino; casinha (direita) = dormir; caixas de frutas/fogueira = comer. |
| Dia/noite | Tint pela hora do PC; 22h–7h rotina noturna: bicho dorme na cama, acorda às 7h. |
| HUD | Balão flutuante sob o mouse, sobre o bicho. Sem HUD fixo. |
| Interações | Clique = carinho (+2 XP, cooldown 30 s, pulo + corações). F2 esconde/mostra a janela. Menu de contexto completo. Ícone na bandeja (GDExtension). |
| Áudio | Nenhum no MVP. |
| Autostart | Fora do MVP (junto com áudio/espécies, depois). |

## 3. Escopo

**Dentro:** mundo top-down com navegação completa; bicho autônomo com 8 direções de
sprite; três zonas (treino, dormir, comer); ciclo dia/noite com rotina de dormir;
tempo real com simulação offline fiel; XP/nível com evolução; carinho por clique;
janela/escala/menu do shell; bandeja do Windows; save persistente.

**Fora:** outras espécies/linhas, áudio, escolha de inicial, ovo chocável, minigames,
banho, combate, loader alternativo (PokeSprite), iniciar com o Windows.

## 4. Arquitetura

Um componente por responsabilidade; tudo em `godot/`, evoluindo o que existe.

```
godot/
├── project.godot                  # autoloads: PetStats, AppWindow
├── assets/
│   ├── garden-map.png             # mapa 1088×896 (convertido da imagem base)
│   └── sprites/0004–0006/         # PMD Collab (já existe)
├── scenes/
│   ├── World.tscn                 # jardim: mapa, navegação, zonas, câmera, tint
│   └── Pokemon.tscn               # bicho (CharacterBody2D + NavigationAgent2D)
├── scripts/
│   ├── pet_stats.gd     (autoload) # stats, relógio real, sim offline, save
│   ├── app_window.gd    (autoload) # janela, menu, F2, escala, bandeja
│   ├── pokemon_controller.gd      # FSM + navegação + 8 direções + carinho
│   ├── pmd_sprite_loader.gd       # 8 direções (hoje: só East/South)
│   ├── activity_zone.gd           # zona declarativa (como hoje)
│   ├── day_night.gd               # tint pela hora do sistema
│   └── hud.gd                     # balão sob o mouse
└── src/tray/                      # GDExtension C++ (só a bandeja)
    ├── tray_extension.cpp         # Shell_NotifyIconW + menu nativo → sinais
    └── SConstruct / .gdextension
```

### Responsabilidades

- **PetStats (autoload)** — dono do estado: fome/energia/XP/nível, taxas de decaimento
  em tempo real, hora do PC, simulação offline, save/load com timestamp da época.
  Único ponto que grava save.
- **AppWindow (autoload)** — dona da janela: sem borda/transparente/topmost, arraste,
  F2, escala, menu de contexto, ícone na bandeja (via GDExtension). Não conhece
  mecânica do jogo.
- **pokemon_controller** — dono do comportamento: FSM
  (VAGAR → DESCANSAR → PRECISAR → BUSCAR → ATIVIDADE), `NavigationAgent2D` para os
  destinos, direção do sprite pela velocidade (8 direções na caminhada), carinho,
  rotina noturna.
- **pmd_sprite_loader** — dono dos sprites: `ANIM_DATA` com as 8 direções (Walk nas
  8; Idle/Eat/Sleep de frente); mantém fallback Idle para o Charizard sem Eat.
- **World.tscn** — dona do lugar: mapa, `NavigationRegion2D` com polígono navegável
  autorado, zonas com props, câmera com limites (0,0–1088,896), `CanvasModulate`.
- **TrayExtension (GDExtension C++)** — dona da bandeja: cria o ícone
  (`Shell_NotifyIconW`), menu nativo (`CreatePopupMenu`/`TrackPopupMenu`), emite
  sinais (`tray_toggle_pressed`, `tray_status_pressed`, `tray_quit_pressed`).
  Sem mecânica de jogo dentro da extensão.

### Fluxo de dados

`PetStats` é a fonte de verdade e emite sinais (`level_changed`, `stats_changed`);
controller e HUD apenas leem/escutam. Save: `user://pet_save.cfg`, auto-save a cada
60 s + ao sair + ao esconder a janela + timestamp (`saved_at_epoch`) para o offline.

## 5. Mecânicas

### Taxas em tempo real (tunáveis, constantes nomeadas)

- Fome: 100 → 0 em ~6 h acordado (0,0046/s); metade disso dormindo
- Energia: 100 → 0 em ~16 h acordado (0,0017/s); na cama, cheio em ~2 h
- Fome < 30 → busca a comida; energia < 20 → busca a cama
- Comer: +6/s na zona (refeição ~20 s); treino: 5 XP/s, visitas de 10–20 s
- De dia, sem necessidade crítica: alterna vagar/descansar/treinar
- Evolução: 100 XP por nível; 17 → Charmeleon; 37 → Charizard (sprites recarregados)

### Dia/noite (hora local do PC)

- 7h–18h pleno; 18h–22h entardecer (tint quente); 22h–7h noite (azul escuro, ~35% de brilho)
- 22h–7h: rotina noturna — vai à cama e dorme; fome < 30 acorda para comer e volta

### Carinho

- Clique no bicho: pulo + corações (partículas), +2 XP, cooldown 30 s

### Simulação offline (fiel, avanço rápido)

- Ao carregar o save: `delta = agora − saved_at_epoch`; < 5 min → nada especial
- Passos de 5 min com as regras da IA: de noite dorme; de dia come se fome baixa,
  senão treina; barato (dias inteiros = milhares de passos, ms de CPU)
- Tetos: XP offline ≤ 300 por absência; fome/energia sempre se resolvem
- Ao reabrir: bicho na casinha, alimentado, XP acumulado

## 6. Shell Windows

- Janela 480×480, sem borda, transparente, sempre no topo (padrão ligado, desligável);
  câmera com limites dentro do mapa
- Escala 0,5×/1×/2×: muda o tamanho da janela (240/480/960); `stretch canvas_items`
  amplia o mesmo recorte (o bicho fica maior, não aparece mais mapa)
- Input: clique esquerdo no bicho = carinho; arrastar com o esquerdo fora do bicho =
  move a janela; clique direito = menu de contexto; F2 = esconder/mostrar (bicho
  continua vivo escondido)
- Menu de contexto: Escala ▸ (0,5×/1×/2×) · Sempre no topo ✓ · Sair (salva e encerra)
- Bandeja (GDExtension): Mostrar/Esconder · Ver status (mostra a janela e aciona o
  balão por alguns segundos) · Sair

## 7. Verificação

- **Testes de unidade (GDScript, headless):** decaimento de stats, XP/nível,
  simulação offline em passos (dia/noite/tetos), round-trip de save; loader de
  sprites (8 direções, fallback Charizard). Rodam com `godot --headless --script`.
- **Verificação manual guiada:** atravessar as duas pontes; buscar comida/cama com
  stats baixas (forçadas por save de teste); carinho e cooldown; dormir de noite e
  acordar às 7h; evolução (save de teste no nível 16/36); escala nas três opções;
  F2 e bandeja; offline (fechar, adiantar o relógio do PC, reabrir).
- **Riscos:** polígono navegável autorado à mão (iterar com o jogo aberto);
  GDExtension exige build no Windows (SCons + MSVC; o CI do repo cobre só a parte
  C++/SDL); sensibilidade do arraste na escala 2×; fuso/hora do PC (offline usa
  época UTC, dia/noite usa hora local).
