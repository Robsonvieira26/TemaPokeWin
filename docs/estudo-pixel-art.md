# Estudo de Pixel Art — TamaPokeWin (Vale Termal)

Levantamento do que já existe, do que falta fazer, e das soluções mais baratas
para cenário, animação e profundidade. Referências: pack **ERW - Ancient Ruins
V 2.3** (cenário), pack **PMD Collab** (pokémon, com `AnimData.xml` de 8
direções) e o projeto **PokePet** (onde as artes de berry já estão salvas).

---

## 1. Sistema de profundidade (o que já funciona)

O contrato do ERW (ver `assets/sprites/erw/godot/LEIA-ME_godot.md`) resolve a
profundidade sem custo de performance:

| Regra | Como está implementado |
|---|---|
| Origem de todo sprite **nos pés** (`centered + offset(0,-h/2)`) | vem pronto na cena ERW; sprites novos devem seguir o padrão |
| **Y-sort** em `LarDoCharmander` (raiz) + `Objetos` | pet ordena por Y junto de árvores/casa/props: atrás quando está acima, na frente quando está abaixo |
| **Superfícies caminháveis fora do sort** (sempre "chão") | `CaminhoDePedras`: pedrinhas, tapete, luz da porta — e agora também escada de pedra, as duas pontes e a cama (o pet passa por cima delas sempre) |
| **Overlays sempre por cima** | `Borboletas` com `z_index = 1` |
| Água animada barata | `CenaOriginal`: mockup achatado 416×400 da cachoeira, 8 quadros a 10 fps — um sprite só, zero tiles animados |

**Regra para arte nova:** objeto sólido → `Objetos` (y-sort); coisa que o pet
pisa/atravessa → `CaminhoDePedras` (ou outro nó fora do sort); efeito aéreo →
`z_index ≥ 1`.

## 2. Levantamento de sprites a fazer

### 2.1 Pet (Charmander → Charmeleon → Charizard)

O pack PMD já entrega **34 animações por estágio** em `PokePet/assets/sprites/pmd/000X/`
(Walk 8-dir, Idle, Eat, Sleep, Wake, Sit, Laying, Hurt, Faint, Hop, Nod, Attack...).
O controller hoje usa só Walk/Idle/Eat/Sleep — o resto é estoque grátis.

| Sprite | Origem | Prioridade | Como fazer barato |
|---|---|---|---|
| Andar 8 direções | **PMD Walk** ✓ já tem | — | — |
| Idle | **PMD Idle** ✓ | — | — |
| Comer (comedouro) | **PMD Eat** ✓ | — | — |
| Dormir na cama | **PMD Sleep/Laying** ✓ | — | — |
| Acordar (estica) | **PMD Wake** ✓ | média | plugar no fim do sono: `Wake` → `Idle` |
| Carinho (reação) | **PMD Nod/Hop** ✓ | média | trocar o pulso de escala por `Hop`/`Nod` |
| **Passar pelas plantas** (grama alta) | fazer | **alta** | **overlay, não sprite do pet:** recortar o topo da grama alta ERW (`tall dry grass ish`) como camada frontal que aparece sobre os pés quando o pet está em vegetação alta + `CPUParticles2D` de folhinhas ao entrar/sair. Pet continua usando Walk |
| Sombra no chão | fazer | alta | 1 elipse 16×8 px semitransparente, `Sprite2D` filho em y=0 — serve para os 3 estágios |
| Comendo berry (frutinha na boca) | fazer | média | 1 frame de override do PMD Eat com uma oran desenhada (10×10) |
| Hurt / desmaio | **PMD Hurt/Faint** ✓ | baixa | para futuros eventos (chuva, treino pesado) |
| Sentar (descanso no tronco) | **PMD Sit** ✓ | baixa | |

### 2.2 Cenário (berrys e comida)

As frutinhas que ele come são **berrys do jogo oficial**. O projeto salvo com
essas artes é o **PokePet** (`~/remote/pokepc/PokePet` — repo
`Robsonvieira26/PokePet`), já usado como fonte do cenário:

| Arquivo (caminho no PokePet) | Tamanho | Uso sugerido |
|---|---|---|
| `assets/sprites/erw/sprites/props/comida/cesta_oran.png` | 30×28 | a **cesta de berrys** do comedouro — apoiar sobre o toco da mesa (826, 678) |
| `.../comida/oran_no_chao.png` | 22×10 | berrys caídas ao redor do comedouro |
| `.../comida/arvore_oran_0.png` | 128×160 | oran berry tree (decoração/estágio futuro) |
| `assets/sprites/erw/sprites/variacoes/arbustos/var_arbusto_baga_oran.png` | 64×32 | arbusto com bagas — trocar de quadro conforme a "estoque" do comedouro |
| `assets/ui/icon_berry.png` | 10×10 | ícone no balão de status (fome) |

Extras de cenário identificados (baratos, reusando o próprio mapa):

| Item | Como fazer barato |
|---|---|
| Coberta na cama quando dorme | 1 sprite 24×16 de manta sobre `cama_aconchegante_1`, visível só de noite |
| Fogueira apagada de dia | a fogueira já é `AnimatedSprite2D`; parar o autoplay e trocar o frame |
| Estátua lendaria (slot 84×96, pé em 140,212) | contrato pronto no LEIA-ME ERW; arte futura |
| Flores/vitas-regias balançando | já coberto pelo shader de vento ERW (`vento.gdshader`) |

### 2.3 UI/efeitos

| Item | Como fazer barato |
|---|---|
| Corações do carinho | hoje são `Label` com "♥"; 1 sprite 8×8 + `CPUParticles2D` |
| Balão de status | já existe (PanelContainer); trocar fundo por 9-slice pixelado quando houver arte |
| Zzz ao dormir | 3 sprites "z" 6×6 subindo com tween (mesma receita dos corações) |

## 3. Soluções baratas por frente

### Cenário
1. **TileMapLayer + autotile** (já em uso — 7 camadas de terreno no ERW). Pintar
   chão novo custa zero código; para bordas novas, copiar tiles existentes ou
   configurar Terrain Sets no painel TileSet.
2. **Mockup achatado animado** (padrão `CenaOriginal`): para uma região viva
   (água, cachoeira), um único sprite animado por cima do terreno custa menos
   que N tiles animados — usar para futuros lagos/riachos.
3. **Shader de vento** (`vento.gdshader`, já na cena): qualquer sprite novo de
   planta ganha movimento carregando um dos 3 ShaderMaterials (árvore/arbusto/rasteira).
4. **Espalhar props por código**: `random_wander_point()` já valida pontos contra
   a navegação; o mesmo truque espalha props com RNG semeada e rejeita posição
   dentro de colisão — decoração procedural sem arte nova.
5. **Reuso de packs**: ERW (cenário) + PMD (pet) cobrem ~90% das necessidades;
   só produzir arte quando o item não existir em nenhum.

### Animação
1. **AnimatedSprite2D + SpriteFrames** (padrão atual): sheet única, 8 linhas de
   direção — o `AnimData.xml` do PMD já define frames/fps de cada animação.
2. **Tweens** (`create_tween`): reações pontuais (coração, pulso, Zzz) sem
   frame a frame.
3. **CPUParticles2D**: folhas na grama alta, poeira, corações — partícula
   quadrada 2–4 px da própria paleta, custo mínimo.
4. **Shaders por pixel**: vento (já), água (mockup), tint dia/noite (`day_night.gd`
   usa `CanvasModulate` — um nó, zero custo por sprite).

### Profundidade
1. Y-sort com origem nos pés (seção 1) — já funcionando.
2. Superfícies caminháveis fora do sort — regra nova desta rodada (escada,
   pontes, cama).
3. `z_index` para overlays (borboletas, grama-frente do pet, coberta da cama).

## 4. Lapidação pendente de colisões e zonas (checklist)

- [ ] **AGENT_RADIUS 4 px vs corpo do pet 12 px**: a navegação deixa o pet colar
      em obstáculos mais que o corpo cabe → raspa em troncos/pedras. Subir para
      8–10 e revalidar spawn/zonas (o teste `test_world` checa pontos navegáveis —
      atualizar os pontos junto).
- [ ] **TrainZone (915, 832) 190×100** é maior que a clareira de pedra; encolher
      para ~140×80 centrada no saco de pancada evita "treinar" de fora da clareira.
- [ ] **EatZone**: trocar o alvo para a posição da cesta de berrys quando o
      comedouro virar cesta_oran.
- [ ] **SleepZone (1020, 648) 120×70** cobre cama + tapete de propósito (o pet
      nasce dormindo) — manter, mas conferir que não vaza para fora da cabana.
- [ ] **Escada de pedra (872, 502)**: área livre por contrato; conferir que o
      caminho ao platô não rente as paredes do penhasco depois do raio novo.
- [ ] Velocidade: 60 → **30 px/s** aplicada nesta rodada; se ainda rápido, 24.

## 5. Próximos passos sugeridos (ordem de custo)

1. Overlay de grama alta + sombra (barato, alto impacto visual).
2. Comedouro com `cesta_oran` + `oran_no_chao` ao redor.
3. `Wake`/`Sit`/`Nod` do PMD plugados nos estados existentes.
4. Coberta da cama + fogueira apagada de dia.
5. Checklist da seção 4 (raio de navegação e zonas).