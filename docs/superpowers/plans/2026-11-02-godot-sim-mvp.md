# Simulação Godot — MVP completo: plano de implementação

> **Para agentic workers:** SUB-SKILL OBRIGATÓRIA: use `superpowers:subagent-driven-development` (recomendado) ou `superpowers:executing-plans` para implementar este plano tarefa por tarefa. Passos usam checkbox (`- [ ]`) para tracking.

**Goal:** transformar a simulação Godot no desktop pet autônomo completo do spec `docs/superpowers/specs/2026-11-02-godot-sim-mvp-design.md`: Charmander vivendo sozinho num jardim top-down 1088×896, tempo real com simulação offline fiel, dia/noite, carinho, shell Windows completo (bandeja via GDExtension).

**Architecture:** evolução in-place do projeto `godot/`. `PetStats` (autoload) fica dono do estado/tempo/save; `AppWindow` (autoload) fica dono da janela/bandeja; `pokemon_controller` fica dono do comportamento (FSM + `NavigationAgent2D` + 8 direções); o mundo vira um mapa único com navegação autorada por retângulos. HUD vira balão de hover.

**Tech Stack:** Godot 4.7 (GL Compatibility), GDScript, GDExtension C++ (SCons + godot-cpp) só para a bandeja do Windows, micro-framework de testes próprio headless.

---

## Notas de execução (leia antes de começar)

- **Onde rodar:** o jogo e os passos manuais rodam no **Windows** com Godot 4.7. Os testes headless precisam do binário `godot` no PATH (WSL ou Windows PowerShell). Se não houver Godot no PATH do WSL, rode os testes no PowerShell:
  `godot --headless --path <repo>\godot --script res://tests/run_all.gd`
- **Desvio aceito do spec:** o mapa entra como `assets/garden-map.jpg` (cópia direta do JPG do Robson) e não `.png` — Godot importa JPG igual, e evita dependência de conversor.
- **Coordenadas do jardim** (zonas, retângulos navegáveis, pontos de vagância) são estimativas iniciais sobre a imagem; a Tarefa 11 (verificação manual) as afina olhando o jogo aberto. Os valores nos passos são concretos, não placeholders.
- **Ordem:** as Tarefas 0–5 são a base testável; 6–8 mecânica e HUD; 9 shell; 10 bandeja (GDExtension); 11 verificação final. TDD em tudo que é lógica pura; passos manuais nos visuais.
- Cada tarefa termina com commit; mensagens seguem o padrão do repo (`feat(godot): ...`), com o trailer `Co-Authored-By: Claude Code <noreply@anthropic.com>`.

## Estrutura de arquivos

```
godot/
├── project.godot                     # sem mudança de autoloads (PetStats, AppWindow já existem)
├── assets/
│   ├── garden-map.jpg                # NOVO — mapa 1088×896
│   ├── egg-scene-source.png          # fica, mas sai do World.tscn
│   └── sprites/0004–0006/            # já existe (PMD Collab)
├── scenes/
│   ├── World.tscn                    # REESCREVER — jardim, zonas nas posições do mapa, tint
│   └── Pokemon.tscn                  # MODIFICAR — NavigationAgent2D, PetArea, balão
├── scripts/
│   ├── pet_stats.gd         (autoload) # REESCREVER — taxas reais, carinho, sim offline, save com época
│   ├── app_window.gd        (autoload) # REESCREVER — escala, F2, menu completo, bandeja
│   ├── pokemon_controller.gd          # REESCREVER — navegação, 8 direções, noite, carinho
│   ├── pmd_sprite_loader.gd           # MODIFICAR — animações Walk por direção
│   ├── activity_zone.gd               # sem mudança
│   ├── world.gd                       # NOVO — polígono navegável + pontos de vagância
│   ├── day_night.gd                   # NOVO — tint pela hora
│   ├── balloon.gd                     # NOVO — HUD de hover
│   ├── hud.gd                         # EXCLUIR (substituído pelo balão)
│   └── sprite_loader.gd               # EXCLUIR (legado, não usado pelo controller)
├── tests/
│   ├── run_all.gd                     # NOVO — runner headless
│   ├── test_base.gd                   # NOVO — micro-framework
│   ├── test_pmd_sprite_loader.gd      # NOVO
│   ├── test_pet_stats.gd              # NOVO
│   └── test_day_night.gd              # NOVO
└── src/tray/                          # NOVO — GDExtension da bandeja
    ├── src/tray_extension.cpp
    ├── SConstruct
    ├── .gdextension
    └── godot-cpp/                     # clone local, ignorado no git
```

---

### Task 0: Framework de testes headless

**Files:**
- Create: `godot/tests/test_base.gd`
- Create: `godot/tests/run_all.gd`
- Create: `godot/tests/test_base_example.gd` (teste do próprio framework, excluído no fim do task)

- [ ] **Step 1: Escrever o framework**

`godot/tests/test_base.gd`:

```gdscript
extends RefCounted
# Base para suites de teste. Cada método test_* roda uma vez; asserts coletam falhas.

var _failures: PackedStringArray = []
var _method := ""

func run() -> int:
	var suite := get_script().resource_path.get_file()
	var failed := 0
	for m in get_method_list():
		if String(m.name).begins_with("test_"):
			var before := _failures.size()
			_method = m.name
			call(m.name)
			if _failures.size() > before:
				failed += 1
				printerr("FAIL  %s.%s" % [suite, m.name])
				for i in range(before, _failures.size()):
					printerr("      " + _failures[i])
			else:
				print("PASS  %s.%s" % [suite, m.name])
	return failed

func _fail(msg: String) -> void:
	_failures.append("%s: %s" % [_method, msg])

func assert_true(cond: bool, msg: String) -> void:
	if not cond:
		_fail(msg)

func assert_eq(got, want, msg := "") -> void:
	if got != want:
		_fail("%s esperado=%s obtido=%s" % [msg, want, got])

func assert_almost(got: float, want: float, eps: float, msg := "") -> void:
	if absf(got - want) > eps:
		_fail("%s esperado≈%s obtido=%s" % [msg, want, got])
```

`godot/tests/run_all.gd`:

```gdscript
# Runner: godot --headless --path godot --script res://tests/run_all.gd
extends SceneTree

func _initialize() -> void:
	var failed := 0
	for f in DirAccess.get_files_at("res://tests"):
		if f.begins_with("test_") and f.ends_with(".gd") and f != "test_base.gd":
			var inst = load("res://tests/" + f).new()
			failed += inst.run()
			inst.free()
	if failed == 0:
		print("OK — todos os testes passaram")
		quit(0)
	else:
		printerr("%d teste(s) falharam" % failed)
		quit(1)
```

- [ ] **Step 2: Escrever um teste de fumaça do framework**

`godot/tests/test_framework.gd`:

```gdscript
extends "res://tests/test_base.gd"

func test_framework_reports_pass() -> void:
	assert_true(true, "sempre verdadeiro")

func test_framework_detects_wrong_eq() -> void:
	# auto-teste: duas afirmações válidas e uma contagem manual
	assert_eq(2 + 2, 4, "aritmética")
	assert_almost(0.1 + 0.2, 0.3, 0.0001, "float")
```

- [ ] **Step 3: Rodar**

Run: `godot --headless --path godot --script res://tests/run_all.gd`
Expected: `PASS  test_framework.test_framework_reports_pass`, `PASS  test_framework.test_framework_detects_wrong_eq` e `OK — todos os testes passaram` (exit 0).

- [ ] **Step 4: Commit**

```bash
git add godot/tests/
git commit -m "test(godot): framework de testes headless

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 1: Mapa do jardim e mundo top-down

**Files:**
- Create: `godot/assets/garden-map.jpg` (cópia da imagem do jardim)
- Create: `godot/scripts/world.gd`
- Rewrite: `godot/scenes/World.tscn`

- [ ] **Step 1: Copiar o mapa**

```bash
cp "/mnt/c/Users/robson.vieira_onfly/Downloads/120363026182455246_g_us-3EB00DE209ED0A9ED30B9B.jpg" godot/assets/garden-map.jpg
file godot/assets/garden-map.jpg   # Esperado: JPEG 1088x896
```

(Se o arquivo não estiver mais em Downloads, pedir a imagem ao Robson — é a arte base do jardim aprovada no brainstorming.)

- [ ] **Step 2: Escrever `world.gd`**

```gdscript
extends Node2D
# Dono do lugar: constrói o polígono navegável a partir de retângulos autorados
# (grama livre + pontes) e expõe os pontos de vagância do bicho.
# O World entra no grupo "world" para o controller achá-lo.

@export var walkable_rects: Array[Rect2] = []
@export var wander_points: PackedVector2Array = []

func _ready() -> void:
	var poly := NavigationPolygon.new()
	for r in walkable_rects:
		poly.add_outline(PackedVector2Array([
			r.position,
			Vector2(r.end.x, r.position.y),
			r.end,
			Vector2(r.position.x, r.end.y),
		]))
	# Deprecated em 4.2+, mas ainda é o caminho de uma linha para outlines de retângulo;
	# o warning no output é esperado e inofensivo.
	poly.make_polygons_from_outlines()
	$NavRegion.navigation_polygon = poly

func random_wander_point() -> Vector2:
	return wander_points.pick_random()
```

- [ ] **Step 3: Reescrever `World.tscn`**

Substituir todo o conteúdo de `godot/scenes/World.tscn` por (o `Tint` entra na
Tarefa 5; o `Balloon` fica no `Pokemon.tscn` da Tarefa 8):

```
[gd_scene load_steps=5 format=3 uid="uid://world_scene"]

[ext_resource type="Texture2D" path="res://assets/garden-map.jpg" id="1"]
[ext_resource type="Script" path="res://scripts/world.gd" id="2"]
[ext_resource type="Script" path="res://scripts/activity_zone.gd" id="3"]
[ext_resource type="PackedScene" path="res://scenes/Pokemon.tscn" id="4"]

[sub_resource type="RectangleShape2D" id="rect_eat"]
size = Vector2(120, 90)

[sub_resource type="RectangleShape2D" id="rect_train"]
size = Vector2(180, 120)

[sub_resource type="RectangleShape2D" id="rect_sleep"]
size = Vector2(110, 80)

[node name="World" type="Node2D" groups=["world"]]
script = ExtResource("2")
walkable_rects = Array[Rect2]([Rect2(32, 96, 440, 240), Rect2(32, 336, 428, 256), Rect2(32, 592, 380, 264), Rect2(495, 45, 140, 60), Rect2(620, 96, 400, 256), Rect2(620, 352, 448, 240), Rect2(620, 592, 456, 288), Rect2(420, 825, 230, 55)])
wander_points = PackedVector2Array(200, 200, 350, 400, 150, 600, 300, 750, 250, 500, 700, 200, 850, 420, 750, 650, 900, 750, 980, 480, 680, 450, 550, 100, 530, 850)

[node name="Map" type="Sprite2D" parent="."]
centered = false
texture = ExtResource("1")

[node name="NavRegion" type="NavigationRegion2D" parent="."]

[node name="EatZone" type="Area2D" parent="." groups=["activity_zones"]]
position = Vector2(905, 660)
script = ExtResource("3")
activity_name = "eat"
xp_per_second = 1.0
stat_restore = "hunger"
restore_rate = 6.0

[node name="EatShape" type="CollisionShape2D" parent="EatZone"]
shape = SubResource("rect_eat")

[node name="TrainZone" type="Area2D" parent="." groups=["activity_zones"]]
position = Vector2(1020, 780)
script = ExtResource("3")
activity_name = "train"
xp_per_second = 5.0
stat_restore = "energy"
restore_rate = -2.0

[node name="TrainShape" type="CollisionShape2D" parent="TrainZone"]
shape = SubResource("rect_train")

[node name="SleepZone" type="Area2D" parent="." groups=["activity_zones"]]
position = Vector2(995, 610)
script = ExtResource("3")
activity_name = "sleep"
xp_per_second = 0.2
stat_restore = "energy"
restore_rate = 8.0

[node name="SleepShape" type="CollisionShape2D" parent="SleepZone"]
shape = SubResource("rect_sleep")

[node name="Pokemon" parent="." instance=ExtResource("4")]
position = Vector2(850, 500)

[node name="Camera" type="Camera2D" parent="."]
position = Vector2(850, 500)
limit_left = 0
limit_top = 0
limit_right = 1088
limit_bottom = 896
position_smoothing_enabled = true
position_smoothing_speed = 3.0
```

(Coordenadas das zonas: EatZone nas caixas de frutas/fogueira, TrainZone no alvo do
dojo, SleepZone na porta da casinha — refinadas na Tarefa 11.)

- [ ] **Step 4: Verificação manual**

Run: abrir o projeto no Godot (Windows) e dar play. Esperado: janela 480×480 mostrando
o jardim com zoom 1×, câmera parada no bicho em (850, 500). O bicho pode vagar "de
forma estranha" (controller antigo fixa `GROUND_Y`) — aceitável até a Tarefa 2.
Sem erros no console de navigation (um warning de `make_polygons_from_outlines`
deprecated é esperado).

- [ ] **Step 5: Commit**

```bash
git add godot/assets/garden-map.jpg godot/scripts/world.gd godot/scenes/World.tscn
git commit -m "feat(godot): jardim top-down com navegação por retângulos

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 2: Loader de sprites com 8 direções (TDD)

**Files:**
- Modify: `godot/scripts/pmd_sprite_loader.gd`
- Test: `godot/tests/test_pmd_sprite_loader.gd`

- [ ] **Step 1: Escrever os testes falhando**

`godot/tests/test_pmd_sprite_loader.gd`:

```gdscript
extends "res://tests/test_base.gd"

func test_direction_for_velocity_cardinals() -> void:
	assert_eq(PmdSpriteLoader.direction_for_velocity(Vector2(0, 1)), 0, "Sul")
	assert_eq(PmdSpriteLoader.direction_for_velocity(Vector2(1, 1)), 1, "SE")
	assert_eq(PmdSpriteLoader.direction_for_velocity(Vector2(1, 0)), 2, "Leste")
	assert_eq(PmdSpriteLoader.direction_for_velocity(Vector2(1, -1)), 3, "NE")
	assert_eq(PmdSpriteLoader.direction_for_velocity(Vector2(0, -1)), 4, "Norte")
	assert_eq(PmdSpriteLoader.direction_for_velocity(Vector2(-1, -1)), 5, "NW")
	assert_eq(PmdSpriteLoader.direction_for_velocity(Vector2(-1, 0)), 6, "Oeste")
	assert_eq(PmdSpriteLoader.direction_for_velocity(Vector2(-1, 1)), 7, "SW")

func test_direction_for_velocity_stopped() -> void:
	assert_eq(PmdSpriteLoader.direction_for_velocity(Vector2.ZERO), 0, "parado = sul")

func test_charmander_has_walk_per_direction() -> void:
	var frames := PmdSpriteLoader.load_pokemon_frames("0004")
	assert_true(frames != null, "frames do Charmander")
	if frames == null:
		return
	for d in 8:
		assert_true(frames.has_animation("Walk%d" % d), "Walk%d existe" % d)
	assert_true(frames.has_animation("Idle"), "Idle existe")
	assert_true(frames.has_animation("Eat"), "Eat existe")
	assert_true(frames.has_animation("Sleep"), "Sleep existe")

func test_charizard_has_no_eat() -> void:
	var frames := PmdSpriteLoader.load_pokemon_frames("0006")
	assert_true(frames != null, "frames do Charizard")
	if frames == null:
		return
	assert_true(not frames.has_animation("Eat"), "Charizard não tem Eat (fallback no controller)")
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `godot --headless --path godot --script res://tests/run_all.gd`
Expected: FAIL — `Invalid call to non-existent function 'direction_for_velocity'` (e os testes de animação falham porque só existe `Walk`).

- [ ] **Step 3: Modificar o loader**

Em `godot/scripts/pmd_sprite_loader.gd`, substituir o bloco de constantes `DIR_*` e
`ANIM_DATA` por (o resto do arquivo — `load_pokemon_frames`, `_load_tex` — muda conforme indicado):

```gdscript
const TICK_RATE := 30.0

# Layout SkyTemple: linha 0 = South (de frente para baixo), cresce no sentido
# horário: 1=SE, 2=East, 3=NE, 4=North, 5=NW, 6=West, 7=SW.
const DIR_SOUTH := 0
const ALL_DIRS := [0, 1, 2, 3, 4, 5, 6, 7]

# Dados MVP das animações por espécie.
# "w"/"h": frame (px). "d": durações em ticks 30fps.
# "dir": linha única (animações de frente). "dirs": cria "<Nome><linha>" por direção.
# null = animação indisponível → controller faz fallback.
const ANIM_DATA: Dictionary = {
	"0004": {  # Charmander
		"Idle":  { "w": 32, "h": 40, "d": [12, 8, 8, 8], "dir": DIR_SOUTH },
		"Walk":  { "w": 32, "h": 32, "d": [6, 8, 6, 8],  "dirs": ALL_DIRS },
		"Sleep": { "w": 32, "h": 24, "d": [30, 35],      "dir": DIR_SOUTH },
		"Eat":   { "w": 24, "h": 32, "d": [6, 8, 6, 8],  "dir": DIR_SOUTH },
	},
	"0005": {  # Charmeleon
		"Idle":  { "w": 32, "h": 56, "d": [40, 2, 3, 3, 3, 2], "dir": DIR_SOUTH },
		"Walk":  { "w": 24, "h": 32, "d": [8, 10, 8, 10],      "dirs": ALL_DIRS },
		"Sleep": { "w": 24, "h": 32, "d": [30, 35],            "dir": DIR_SOUTH },
		"Eat":   { "w": 24, "h": 32, "d": [6, 8, 6, 8],        "dir": DIR_SOUTH },
	},
	"0006": {  # Charizard
		"Idle":  { "w": 40, "h": 48, "d": [15, 15, 15, 15], "dir": DIR_SOUTH },
		"Walk":  { "w": 40, "h": 48, "d": [8, 10, 8, 10],   "dirs": ALL_DIRS },
		"Sleep": { "w": 32, "h": 48, "d": [30, 35],         "dir": DIR_SOUTH },
		"Eat":   null,  # indisponível no PMD Collab — controller usa Idle como fallback
	},
}

# Velocidade → direção SkyTemple. angle() em GDScript: 0 = Leste, +90 = Sul
# (y cresce para baixo). A linha cresce no sentido horário a partir do Sul.
static func direction_for_velocity(v: Vector2) -> int:
	if v.length_squared() < 0.0001:
		return DIR_SOUTH
	var a := rad_to_deg(v.angle())
	return wrapi(roundi((90.0 - a) / 45.0), 0, 8)
```

E substituir o corpo de `load_pokemon_frames` (a parte do `for anim_name`) por:

```gdscript
	for anim_name: String in data:
		var spec = data[anim_name]
		if spec == null:
			continue
		var path := "%s/%s-Anim.png" % [base, anim_name]
		var tex := _load_tex(path)
		if tex == null:
			push_warning("PmdSpriteLoader: arquivo não encontrado: %s" % path)
			continue
		if spec.has("dirs"):
			for row: int in spec["dirs"]:
				_add_anim(frames, "%s%d" % [anim_name, row], tex, spec, row)
		else:
			_add_anim(frames, anim_name, tex, spec, spec["dir"])
```

E mudar a assinatura/corpo de `_add_anim` (agora recebe a linha explicitamente):

```gdscript
static func _add_anim(
	frames: SpriteFrames,
	anim_name: String,
	texture: Texture2D,
	spec: Dictionary,
	row: int
) -> void:
	frames.add_animation(anim_name)
	frames.set_animation_loop(anim_name, true)
	frames.set_animation_speed(anim_name, TICK_RATE)

	var fw: int     = spec["w"]
	var fh: int     = spec["h"]
	var durs: Array = spec["d"]
	var y := row * fh

	for i: int in durs.size():
		var atlas := AtlasTexture.new()
		atlas.atlas  = texture
		atlas.region = Rect2(i * fw, y, fw, fh)
		frames.add_frame(anim_name, atlas, float(durs[i]))
```

(Suposição mantida do código atual: cada sheet PMD tem grade uniforme — o mesmo
`w`/`h` vale para as 8 linhas. Se uma sheet real desrespeitar isso, ajusta-se o
`w/h` da espécie na Tarefa 11 com o jogo aberto.)

- [ ] **Step 4: Rodar e ver passar**

Run: `godot --headless --path godot --script res://tests/run_all.gd`
Expected: PASS nos 4 testes do loader + PASS nos do framework; exit 0.

- [ ] **Step 5: Commit**

```bash
git add godot/scripts/pmd_sprite_loader.gd godot/tests/test_pmd_sprite_loader.gd
git commit -m "feat(godot): sprites PMD nas 8 direções

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 3: Navegação e FSM top-down no controller

**Files:**
- Modify: `godot/scripts/pokemon_controller.gd` (reescrever)
- Modify: `godot/scripts/world.gd` (câmera passa a seguir o bicho aqui)
- Rewrite: `godot/scenes/Pokemon.tscn`

- [ ] **Step 1: Adicionar o `NavAgent` ao `Pokemon.tscn`**

Substituir todo o conteúdo de `godot/scenes/Pokemon.tscn` por:

```
[gd_scene load_steps=3 format=3 uid="uid://pokemon_scene"]

[ext_resource type="Script" path="res://scripts/pokemon_controller.gd" id="1"]

[sub_resource type="CircleShape2D" id="body_shape"]
radius = 12.0

[node name="Pokemon" type="CharacterBody2D"]
motion_mode = 1
script = ExtResource("1")

[node name="Shape" type="CollisionShape2D" parent="."]
shape = SubResource("body_shape")

[node name="Visual" type="AnimatedSprite2D" parent="."]
position = Vector2(0, -18)

[node name="NavAgent" type="NavigationAgent2D" parent="."]
path_desired_distance = 8.0
target_desired_distance = 10.0
radius = 14.0
```

(`motion_mode = 1` = FLOATING: movimento top-down sem gravidade.)

- [ ] **Step 2: Câmera segue o bicho via `world.gd`**

Adicionar ao final de `godot/scripts/world.gd`:

```gdscript
func _process(_delta: float) -> void:
	$Camera.position = $Pokemon.position
```

- [ ] **Step 3: Reescrever o controller**

Substituir todo o conteúdo de `godot/scripts/pokemon_controller.gd` por:

```gdscript
extends CharacterBody2D
# Comportamento do bicho: FSM autônoma + navegação pelo jardim + 8 direções.
# Sprites: PMD Collab via PmdSpriteLoader; espécie pelo nível (1/17/37).
# As necessidades se resolvem sozinhas: fome baixa → tigela; energia baixa → cama.

enum State { WANDER, IDLE, ACTIVITY, SEEK }

const WALK_SPEED := 60.0
const HUNGER_LOW := 30.0   # fome abaixo disso → busca comida
const ENERGY_LOW := 20.0   # energia abaixo disso → busca a cama

var state:       State = State.WANDER
var state_timer: float = 0.0
var active_zone: Node  = null
var _seek_zone:  Node  = null
var _facing:     int   = PmdSpriteLoader.DIR_SOUTH
var _species_id: String = "0004"
var _last_level: int    = 1

@onready var nav: NavigationAgent2D = $NavAgent
@onready var sprite: AnimatedSprite2D = $Visual

func _ready() -> void:
	add_to_group("pet")
	_load_sprites()
	_set_state(State.WANDER)

func _physics_process(delta: float) -> void:
	# NavigationServer só sincroniza o mapa depois do primeiro passo de física
	if NavigationServer2D.map_get_iteration_id(nav.get_navigation_map()) == 0:
		return
	state_timer -= delta
	if PetStats.level != _last_level:
		_last_level = PetStats.level
		_load_sprites()
	match state:
		State.WANDER:   _do_wander()
		State.IDLE:     _do_idle()
		State.ACTIVITY: _do_activity(delta)
		State.SEEK:     _do_seek()

func _world() -> Node:
	return get_tree().get_first_node_in_group("world")

func _do_wander() -> void:
	if _navigate():
		_set_state(State.IDLE)
	elif state_timer <= 0.0:
		nav.target_position = _world().random_wander_point()
		state_timer = 10.0

func _do_idle() -> void:
	velocity = Vector2.ZERO
	if state_timer > 0.0:
		return
	var need := _check_need()
	if need != "":
		_start_seek(need)
	else:
		_set_state(State.WANDER)

func _do_activity(delta: float) -> void:
	velocity = Vector2.ZERO
	if active_zone:
		PetStats.add_xp(active_zone.xp_per_second * delta, active_zone.activity_name)
		match active_zone.stat_restore:
			"hunger": PetStats.restore_hunger(active_zone.restore_rate * delta)
			"energy": PetStats.restore_energy(active_zone.restore_rate * delta)
	if state_timer <= 0.0:
		_set_state(State.WANDER)

func _do_seek() -> void:
	if _navigate():
		_set_state(State.IDLE)  # chegou; a zona dispara enter_activity pelo body_entered
	elif state_timer <= 0.0:
		_set_state(State.WANDER)  # desistiu

# Navega um passo. Devolve true quando chegou ao destino.
func _navigate() -> bool:
	if nav.is_navigation_finished():
		velocity = Vector2.ZERO
		return true
	var next := nav.get_next_path_position()
	velocity = (next - global_position).normalized() * WALK_SPEED
	move_and_slide()
	_face(velocity)
	return false

func _check_need() -> String:
	if PetStats.hunger < HUNGER_LOW:
		return "eat"
	if PetStats.energy < ENERGY_LOW:
		return "sleep"
	return ""

func _start_seek(zone_name: String) -> void:
	for zone: Node in get_tree().get_nodes_in_group("activity_zones"):
		if zone.get("activity_name") == zone_name:
			_seek_zone = zone
			nav.target_position = (zone as Node2D).global_position
			state       = State.SEEK
			state_timer = 30.0
			return
	_set_state(State.WANDER)  # zona não encontrada

func _set_state(new_state: State) -> void:
	state = new_state
	match new_state:
		State.WANDER:
			state_timer = 10.0
			if _world():
				nav.target_position = _world().random_wander_point()
			_play_anim("Walk%d" % _facing)
		State.IDLE:
			state_timer = randf_range(1.5, 4.0)
			_play_anim("Idle")
		State.ACTIVITY:
			state_timer = randf_range(4.0, 10.0)
			# animação da atividade definida em enter_activity()
		State.SEEK:
			pass  # configurado em _start_seek

func enter_activity(zone: Node) -> void:
	active_zone = zone
	_set_state(State.ACTIVITY)
	match zone.activity_name:
		"eat":   _play_anim("Eat")
		"sleep": _play_anim("Sleep")
		_:       _play_anim("Idle")

func exit_activity(_zone: Node) -> void:
	if active_zone == _zone:
		active_zone = null
		_set_state(State.WANDER)

# ── Sprites ──────────────────────────────────────────────────────────

func _load_sprites() -> void:
	var new_species := _species_for_level(PetStats.level)
	if new_species == _species_id and sprite != null and sprite.sprite_frames != null:
		return
	_species_id = new_species
	var frames := PmdSpriteLoader.load_pokemon_frames(_species_id)
	if frames and sprite:
		sprite.sprite_frames = frames

func _face(v: Vector2) -> void:
	_facing = PmdSpriteLoader.direction_for_velocity(v)

func _play_anim(anim_name: String) -> void:
	if sprite == null or sprite.sprite_frames == null:
		return
	if not sprite.sprite_frames.has_animation(anim_name):
		anim_name = "Idle"  # fallback (ex.: Charizard sem Eat)
	if not sprite.sprite_frames.has_animation(anim_name):
		return
	if sprite.animation != anim_name:
		sprite.play(anim_name)

static func _species_for_level(lvl: int) -> String:
	if lvl >= 37:
		return "0006"
	if lvl >= 17:
		return "0005"
	return "0004"
```

- [ ] **Step 4: Verificação manual**

Run: dar play no projeto. Esperado:
- O bicho anda pelo jardim com sprite nas 8 direções (virando ao mudar de rumo),
  desvia do rio e das árvores, e ocasionalmente atravessa as pontes.
- Com as taxas antigas de decaimento (fome 0,5/s), ele busca a tigela em ~1–2 min,
  come (anim Eat), depois volta a vagar. Nenhum erro de navigation no console.

- [ ] **Step 5: Commit**

```bash
git add godot/scripts/pokemon_controller.gd godot/scripts/world.gd godot/scenes/Pokemon.tscn
git commit -m "feat(godot): navegação autônoma pelo jardim com 8 direções

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 4: PetStats em tempo real (TDD)

**Files:**
- Modify: `godot/scripts/pet_stats.gd` (reescrever)
- Test: `godot/tests/test_pet_stats.gd`

- [ ] **Step 1: Escrever os testes falhando**

`godot/tests/test_pet_stats.gd`:

```gdscript
extends "res://tests/test_base.gd"

func _fresh() -> Node:
	# Instância direta do script (sem passar pelo autoload): _ready não roda,
	# então nada toca no save real do usuário.
	var ps := load("res://scripts/pet_stats.gd").new()
	ps.utc_offset_hours = 0.0
	ps.save_path = "user://test_save.cfg"
	return ps

func test_tick_decays_and_restores() -> void:
	var ps := _fresh()
	ps.tick(3600.0)
	assert_almost(ps.hunger, 100.0 - 0.00463 * 3600.0, 0.01, "fome após 1 h acordado")
	assert_almost(ps.energy, 100.0 - 0.00174 * 3600.0, 0.01, "energia após 1 h acordado")
	ps.asleep = true
	ps.tick(7200.0)
	assert_almost(ps.energy, 100.0, 0.01, "energia cheia após 2 h de cama")
	ps.free()

func test_add_xp_levels_and_signal() -> void:
	var ps := _fresh()
	var levels := []
	ps.level_changed.connect(func(l): levels.append(l))
	ps.add_xp(150.0, "teste")
	assert_eq(ps.level, 2, "150 XP = nível 2")
	assert_eq(levels.size(), 1, "um level_changed disparado")
	ps.free()

func test_try_pet_cooldown_and_xp() -> void:
	var ps := _fresh()
	assert_true(ps.try_pet(10.0), "primeiro carinho conta")
	assert_eq(ps.xp, 2.0, "carinho vale +2 XP")
	assert_true(not ps.try_pet(20.0), "dentro do cooldown não conta")
	assert_true(ps.try_pet(41.0), "após 30 s conta de novo")
	ps.free()

func test_is_night() -> void:
	var ps := _fresh()
	assert_true(ps.is_night(23.0), "23 h é noite")
	assert_true(ps.is_night(3.0), "3 h é noite")
	assert_true(not ps.is_night(12.0), "12 h é dia")
	assert_true(not ps.is_night(21.9), "21,9 h ainda é dia")
	ps.free()

func test_local_hour_with_offset() -> void:
	var ps := _fresh()
	ps.utc_offset_hours = -3.0
	assert_almost(ps._local_hour(12.0 * 3600.0), 9.0, 0.001, "12 h UTC com fuso -3 = 9 h local")
	ps.free()

func test_offline_overnight_sleeps_and_eats() -> void:
	var ps := _fresh()
	ps.hunger = 100.0
	ps.energy = 40.0
	var start := 21.0 * 3600.0  # 21:00, simulação até 07:00 do dia seguinte
	ps.simulate_offline(start, start + 10.0 * 3600.0)
	assert_true(ps.energy >= 95.0, "dormiu a noite inteira")
	assert_true(ps.hunger > 50.0, "café da manhã resolveu a fome")
	ps.free()

func test_offline_xp_cap() -> void:
	var ps := _fresh()
	var start := 12.0 * 3600.0  # 48 h de tempo diurno simulado
	ps.simulate_offline(start, start + 48.0 * 3600.0)
	assert_almost(ps.xp, 300.0, 0.001, "XP offline limitado ao teto de 300")
	ps.free()

func test_save_load_roundtrip() -> void:
	var ps := _fresh()
	ps.add_xp(40.0, "teste")
	ps.hunger = 55.0
	ps.save_data()
	var ps2 := _fresh()
	var saved_at := ps2.load_data()
	assert_eq(ps2.level, 1, "nível persistido")
	assert_almost(ps2.xp, 40.0, 0.001, "xp persistido")
	assert_almost(ps2.hunger, 55.0, 0.001, "fome persistida")
	assert_true(saved_at > 0.0, "época do save gravada")
	ps.free()
	ps2.free()
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `godot --headless --path godot --script res://tests/run_all.gd`
Expected: FAIL — `Invalid call to non-existent function 'tick'` no primeiro teste.

- [ ] **Step 3: Reescrever `pet_stats.gd`**

Substituir todo o conteúdo de `godot/scripts/pet_stats.gd` por:

```gdscript
extends Node
# Autoload: PetStats — dono do estado do bicho (fome, energia, XP/nível),
# do relógio do mundo e do save. Fonte única de verdade: os outros nós só
# leem daqui, escutam os sinais ou pedem ações (try_pet, restore_*).

signal stats_changed
signal level_changed(new_level: int)

const SAVE_PATH := "user://pet_save.cfg"

# ── Tuning (spec, seção 5) ────────────────────────────────────────────
const XP_PER_LEVEL         := 100.0
const HUNGER_DECAY_AWAKE   := 0.00463  # 100 → 0 em ~6 h acordado
const HUNGER_DECAY_ASLEEP  := 0.002315 # dormindo, metade do ritmo
const ENERGY_DECAY         := 0.00174  # 100 → 0 em ~16 h acordado
const ENERGY_RESTORE_SLEEP := 0.0139   # 0 → 100 em ~2 h de cama
const HUNGER_LOW           := 30.0
const ENERGY_LOW           := 20.0
const PET_XP               := 2.0      # carinho
const PET_COOLDOWN         := 30.0     # s entre carinhos que contam
const NIGHT_START          := 22.0     # hora local
const NIGHT_END            := 7.0
const OFFLINE_STEP         := 300.0    # s por passo da simulação offline
const OFFLINE_MIN          := 300.0    # absências menores que isso são ignoradas
const OFFLINE_XP_CAP       := 300.0    # teto de XP por absência (~3 níveis)
const OFFLINE_XP_PER_STEP  := 1.0

var xp: float     = 0.0
var level: int    = 1
var hunger: float = 100.0
var energy: float = 100.0
var asleep := false            # setado pelo controller (bicho na cama)
var utc_offset_hours := 0.0    # fuso local; _ready calcula, testes sobrescrevem
var save_path: String = SAVE_PATH

var _pet_ready_at := -INF  # engine-time (s) em que o próximo carinho conta
var _auto_save: Timer

func _ready() -> void:
	utc_offset_hours = _system_utc_offset()
	var saved_at := load_data()
	if saved_at > 0.0:
		simulate_offline(saved_at, now_epoch())
	_auto_save = Timer.new()
	_auto_save.wait_time = 60.0
	_auto_save.autostart = true
	_auto_save.timeout.connect(save_data)
	add_child(_auto_save)

func _process(delta: float) -> void:
	tick(delta)

# Decaimento em tempo real. O controller liga `asleep` quando o bicho está na cama.
func tick(delta: float) -> void:
	if asleep:
		hunger = maxf(0.0,  hunger - HUNGER_DECAY_ASLEEP * delta)
		energy = minf(100.0, energy + ENERGY_RESTORE_SLEEP * delta)
	else:
		hunger = maxf(0.0,  hunger - HUNGER_DECAY_AWAKE * delta)
		energy = maxf(0.0,  energy - ENERGY_DECAY * delta)
	stats_changed.emit()

func add_xp(amount: float, source: String = "") -> void:
	xp += amount
	while xp >= XP_PER_LEVEL:
		xp    -= XP_PER_LEVEL
		level += 1
		level_changed.emit(level)
		print("[PetStats] Level up! Nível ", level, " (fonte: ", source, ")")

func restore_hunger(amount: float) -> void:
	hunger = minf(100.0, hunger + amount)

func restore_energy(amount: float) -> void:
	energy = minf(100.0, energy + amount)

# Carinho: devolve true se o carinho contou (fora do cooldown).
func try_pet(now_sec: float) -> bool:
	if now_sec < _pet_ready_at:
		return false
	_pet_ready_at = now_sec + PET_COOLDOWN
	add_xp(PET_XP, "carinho")
	return true

func is_night(hour: float) -> bool:
	return hour >= NIGHT_START or hour < NIGHT_END

func now_epoch() -> float:
	return Time.get_unix_time_from_system()

# ── Offline (spec, seção 5): a IA em avanço rápido ────────────────────
# Passos de OFFLINE_STEP s com as regras da IA: de noite dorme; de dia come
# se a fome está baixa, senão treina (XP com teto). Necessidades sempre se
# resolvem — o bicho nunca fica "ferido" nem morre.
func simulate_offline(start_epoch: float, now_epoch: float) -> void:
	var elapsed := now_epoch - start_epoch
	if elapsed < OFFLINE_MIN:
		return
	var xp_offline := 0.0
	var steps := int(elapsed / OFFLINE_STEP)
	for i in steps:
		var hour := _local_hour(start_epoch + (i + 1) * OFFLINE_STEP)
		if is_night(hour):
			energy = minf(100.0, energy + ENERGY_RESTORE_SLEEP * OFFLINE_STEP)
			hunger = maxf(0.0,  hunger - HUNGER_DECAY_ASLEEP * OFFLINE_STEP)
		elif hunger < HUNGER_LOW:
			hunger = 100.0  # comeu a refeição inteira no passo
		elif xp_offline < OFFLINE_XP_CAP:
			xp_offline += OFFLINE_XP_PER_STEP
			add_xp(OFFLINE_XP_PER_STEP, "treino offline")
	asleep = false
	if energy < ENERGY_LOW:
		energy = 100.0  # chegou da absência descansado
	stats_changed.emit()

func _local_hour(epoch: float) -> float:
	return fposmod(epoch / 3600.0 + utc_offset_hours, 24.0)

# Fuso local do sistema, a partir da diferença relógio local × UTC.
func _system_utc_offset() -> float:
	var local := Time.get_time_dict_from_system()
	var utc   := Time.get_time_dict_from_system(true)
	var diff  := (float(local.hour) + local.minute / 60.0)
		- (float(utc.hour) + utc.minute / 60.0)
	return fposmod(diff + 12.0, 24.0) - 12.0

# ── Save ──────────────────────────────────────────────────────────────
func save_data() -> void:
	var cfg := ConfigFile.new()
	cfg.set_value("stats", "xp", xp)
	cfg.set_value("stats", "level", level)
	cfg.set_value("stats", "hunger", hunger)
	cfg.set_value("stats", "energy", energy)
	cfg.set_value("stats", "saved_at_epoch", now_epoch())
	cfg.save(save_path)

# Devolve a época (UTC) do último save; 0 se não havia save.
func load_data() -> float:
	var cfg := ConfigFile.new()
	if cfg.load(save_path) != OK:
		return 0.0
	xp     = cfg.get_value("stats", "xp", 0.0)
	level  = cfg.get_value("stats", "level", 1)
	hunger = maxf(HUNGER_LOW, cfg.get_value("stats", "hunger", 100.0))
	energy = maxf(ENERGY_LOW, cfg.get_value("stats", "energy", 100.0))
	return cfg.get_value("stats", "saved_at_epoch", 0.0)
```

- [ ] **Step 4: Rodar e ver passar**

Run: `godot --headless --path godot --script res://tests/run_all.gd`
Expected: PASS em todas as suítes (framework, loader, pet_stats); exit 0.

- [ ] **Step 5: Commit**

```bash
git add godot/scripts/pet_stats.gd godot/tests/test_pet_stats.gd
git commit -m "feat(godot): PetStats em tempo real com sim offline fiel

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 5: Ciclo dia/noite — tint (TDD na lógica, visual manual)

**Files:**
- Create: `godot/scripts/day_night.gd`
- Modify: `godot/scenes/World.tscn` (nó `Tint`)
- Test: `godot/tests/test_day_night.gd`

- [ ] **Step 1: Escrever os testes falhando**

`godot/tests/test_day_night.gd`:

```gdscript
extends "res://tests/test_base.gd"

func test_tint_day_is_white() -> void:
	assert_eq(DayNight.tint_for_hour(12.0), DayNight.COL_DAY, "meio-dia sem tint")

func test_tint_night_dark() -> void:
	assert_eq(DayNight.tint_for_hour(23.0), DayNight.COL_NIGHT, "23 h escuro")
	assert_eq(DayNight.tint_for_hour(2.0), DayNight.COL_NIGHT, "2 h escuro")

func test_tint_dusk_between() -> void:
	var c := DayNight.tint_for_hour(19.0)
	assert_true(c.r > DayNight.COL_NIGHT.r and c.r < DayNight.COL_DAY.r,
		"19 h entre o dia e a noite")

func test_tint_morning_warms_up() -> void:
	var c := DayNight.tint_for_hour(7.5)
	assert_true(c.r > DayNight.COL_NIGHT.r, "7:30 já mais claro que a noite")
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `godot --headless --path godot --script res://tests/run_all.gd`
Expected: FAIL — identifier `DayNight` não encontrado (classe não existe).

- [ ] **Step 3: Implementar `day_night.gd`**

`godot/scripts/day_night.gd`:

```gdscript
class_name DayNight
extends CanvasModulate
# Tint da hora local do sistema sobre o jardim (spec, seção 5).
# 7h–8h manhã · 8h–18h pleno · 18h–20h entardecer · 20h–22h anoitecer · 22h–7h noite.

const COL_DAY   := Color(1.0, 1.0, 1.0)
const COL_DUSK  := Color(1.0, 0.72, 0.55)   # entardecer quente
const COL_NIGHT := Color(0.35, 0.42, 0.58)  # azul escuro (~35% de brilho)

# Pura e testável: cor do tint para a hora local dada.
static func tint_for_hour(hour: float) -> Color:
	if hour >= 8.0 and hour < 18.0:
		return COL_DAY
	if hour >= 18.0 and hour < 20.0:
		return COL_DAY.lerp(COL_DUSK, hour - 18.0)
	if hour >= 20.0 and hour < 22.0:
		return COL_DUSK.lerp(COL_NIGHT, hour - 20.0)
	if hour >= 22.0 or hour < 7.0:
		return COL_NIGHT
	return COL_NIGHT.lerp(COL_DAY, hour - 7.0)  # 7h–8h: amanhecer

func _process(_delta: float) -> void:
	var t := Time.get_time_dict_from_system()
	var hour := float(t.hour) + t.minute / 60.0
	modulate = tint_for_hour(hour)
```

- [ ] **Step 4: Colocar o `Tint` no mundo**

Em `godot/scenes/World.tscn`:
- mudar `load_steps=5` para `load_steps=6`;
- adicionar após a última linha `[ext_resource ...]`:

```
[ext_resource type="Script" path="res://scripts/day_night.gd" id="5"]
```

- adicionar ao final do arquivo:

```
[node name="Tint" type="CanvasModulate" parent="."]
script = ExtResource("5")
```

- [ ] **Step 5: Rodar os testes**

Run: `godot --headless --path godot --script res://tests/run_all.gd`
Expected: PASS em todas as suítes; exit 0.

- [ ] **Step 6: Verificação manual**

Run: dar play. Esperado: o jardim é desenhado com a cor da hora atual do PC
(pleno de dia). No editor, aba **Remote**, selecionar o nó `World/Tint` e conferir
que `modulate` muda se a hora local estiver perto de uma transição.

- [ ] **Step 7: Commit**

```bash
git add godot/scripts/day_night.gd godot/scenes/World.tscn godot/tests/test_day_night.gd
git commit -m "feat(godot): ciclo dia/noite com tint pela hora do PC

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 6: Rotina noturna e flag `asleep` (TDD na hora, manual no comportamento)

**Files:**
- Modify: `godot/scripts/pet_stats.gd` (`local_hour_now` + `hour_override`)
- Modify: `godot/scripts/pokemon_controller.gd`
- Test: `godot/tests/test_pet_stats.gd` (novo teste)

- [ ] **Step 1: Escrever o teste falhando**

Adicionar ao final de `godot/tests/test_pet_stats.gd`:

```gdscript
func test_hour_override_for_debug() -> void:
	var ps := _fresh()
	ps.hour_override = 23.0
	assert_almost(ps.local_hour_now(), 23.0, 0.001, "override de hora (debug/remoto)")
	ps.hour_override = 12.0
	assert_almost(ps.local_hour_now(), 12.0, 0.001, "override segue a hora forçada")
	ps.free()
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `godot --headless --path godot --script res://tests/run_all.gd`
Expected: FAIL — `local_hour_now` não existe.

- [ ] **Step 3: Implementar em `pet_stats.gd`**

Adicionar junto às variáveis (após `save_path`):

```gdscript
var hour_override := -1.0  # >= 0 força a hora local (debug via aba Remote do editor)
```

E adicionar os métodos (junto a `_local_hour`):

```gdscript
# Hora local do mundo agora — respeita o override de debug.
func local_hour_now() -> float:
	if hour_override >= 0.0:
		return hour_override
	return _local_hour(now_epoch())
```

- [ ] **Step 4: Rotina noturna no controller**

Em `godot/scripts/pokemon_controller.gd`:

Substituir `_check_need` por (de noite, dormir vale como necessidade, a menos
que a fome esteja crítica — aí come primeiro):

```gdscript
func _check_need() -> String:
	if PetStats.hunger < HUNGER_LOW:
		return "eat"
	if PetStats.energy < ENERGY_LOW or PetStats.is_night(PetStats.local_hour_now()):
		return "sleep"
	return ""
```

Substituir `_do_activity` por (de noite, deitado na cama, segura o sono até
amanhecer ou a fome acordá-lo):

```gdscript
func _do_activity(delta: float) -> void:
	velocity = Vector2.ZERO
	if active_zone:
		PetStats.add_xp(active_zone.xp_per_second * delta, active_zone.activity_name)
		match active_zone.stat_restore:
			"hunger": PetStats.restore_hunger(active_zone.restore_rate * delta)
			"energy": PetStats.restore_energy(active_zone.restore_rate * delta)
		if active_zone.activity_name == "sleep" \
				and PetStats.is_night(PetStats.local_hour_now()) \
				and PetStats.hunger >= HUNGER_LOW:
			state_timer = maxf(state_timer, 1.0)  # segura na cama
			PetStats.asleep = true
	if state_timer <= 0.0:
		PetStats.asleep = false
		_set_state(State.WANDER)
```

E no `_set_state`, logo após `state = new_state`, garantir que qualquer troca
de estado acorda o bicho:

```gdscript
	PetStats.asleep = false
```

- [ ] **Step 5: Rodar e ver passar**

Run: `godot --headless --path godot --script res://tests/run_all.gd`
Expected: PASS em todas as suítes; exit 0.

- [ ] **Step 6: Verificação manual**

Run: dar play. No editor, aba **Remote**, setar `PetStats.hour_override = 23.0`.
Esperado: o bicho caminha até a casinha, entra na zona de dormir, anim Sleep
e fica lá (decai fome devagar). Setar `hour_override = 12.0`: ele acorda e
volta a vagar. Setar `hour_override = 23.0` e esperar/forçar a fome para baixo
de 30 (Remote: `PetStats.hunger = 10.0`): ele acorda, come e volta para a cama.

- [ ] **Step 7: Commit**

```bash
git add godot/scripts/pet_stats.gd godot/scripts/pokemon_controller.gd godot/tests/test_pet_stats.gd
git commit -m "feat(godot): rotina noturna — dorme às 22h, acorda às 7h

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 7: Carinho por clique

**Files:**
- Modify: `godot/scenes/Pokemon.tscn` (`PetArea`)
- Modify: `godot/scripts/pokemon_controller.gd`
- Test: a lógica de cooldown/XP já é testada (`test_try_pet_cooldown_and_xp`, Task 4); aqui é integração de input + reação visual (manual)

- [ ] **Step 1: Área clicável no `Pokemon.tscn`**

Em `godot/scenes/Pokemon.tscn`:
- mudar `load_steps=3` para `load_steps=4`;
- adicionar antes da primeira linha `[node ...]`:

```
[sub_resource type="CircleShape2D" id="hover_shape"]
radius = 26.0
```

- adicionar ao final:

```
[node name="PetArea" type="Area2D" parent="."]
input_pickable = true

[node name="PetAreaShape" type="CollisionShape2D" parent="PetArea"]
shape = SubResource("hover_shape")
```

- [ ] **Step 2: Ligar input e reação no controller**

Em `godot/scripts/pokemon_controller.gd`:

Adicionar a variável (junto às outras):

```gdscript
var hovered := false  # cursor sobre o bicho (usado pelo balão e pelo arrasto da janela)
```

No `_ready()`, após `add_to_group("pet")`:

```gdscript
	$PetArea.input_event.connect(_on_pet_input)
	$PetArea.mouse_entered.connect(func(): hovered = true)
	$PetArea.mouse_exited.connect(func(): hovered = false)
```

E adicionar a seção de carinho ao final do arquivo:

```gdscript
# ── Carinho ──────────────────────────────────────────────────────────

func _on_pet_input(_viewport: Node, event: InputEvent, _shape_idx: int) -> void:
	if event is InputEventMouseButton \
			and event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
		_pet()

func _pet() -> void:
	if PetStats.try_pet(Time.get_ticks_msec() / 1000.0):
		_react()

func _react() -> void:
	# pulso de escala + corações flutuando
	var tw := create_tween()
	tw.tween_property(sprite, "scale", Vector2(1.15, 1.15), 0.08)
	tw.tween_property(sprite, "scale", Vector2.ONE, 0.12)
	for i in 3:
		_heart(Vector2(randf_range(-14.0, 14.0), -34.0))

func _heart(at: Vector2) -> void:
	var lbl := Label.new()
	lbl.text = "♥"
	lbl.modulate = Color(1.0, 0.42, 0.62)
	lbl.add_theme_font_size_override("font_size", 12)
	lbl.position = at
	add_child(lbl)
	var tw := create_tween()
	tw.tween_property(lbl, "position", at + Vector2(0, -34), 0.9)
	tw.parallel().tween_property(lbl, "modulate:a", 0.0, 0.9)
	tw.tween_callback(lbl.queue_free)
```

- [ ] **Step 3: Verificação manual**

Run: dar play e clicar no bicho repetidamente. Esperado: pulso de escala +
corações no primeiro clique; cliques em menos de 30 s não somem XP (observar
pelo balão na Task 8 ou pelo print de level up). Clique fora do bicho não reage.

- [ ] **Step 4: Commit**

```bash
git add godot/scenes/Pokemon.tscn godot/scripts/pokemon_controller.gd
git commit -m "feat(godot): carinho por clique com reação e cooldown

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 8: Balão do HUD e limpeza

**Files:**
- Create: `godot/scripts/balloon.gd`
- Modify: `godot/scenes/Pokemon.tscn` (nó `Balloon`)
- Modify: `godot/scenes/World.tscn` (remove o HUD fixo)
- Delete: `godot/scripts/hud.gd`
- Delete: `godot/scripts/sprite_loader.gd` (legado, sem uso desde PR #5)

- [ ] **Step 1: Escrever `balloon.gd`**

`godot/scripts/balloon.gd`:

```gdscript
extends PanelContainer
# Balão do HUD: aparece com o cursor sobre o bicho (ou por alguns segundos
# quando o jogador pede "Ver status"). Filho do Pokemon (espaço do mundo).

var _hunger: ProgressBar
var _energy: ProgressBar
var _xp: ProgressBar
var _level: Label
var _until_msec := -1

func _ready() -> void:
	visible = false
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	custom_minimum_size = Vector2(120, 66)
	position = Vector2(-60, -100)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 3)
	add_child(vbox)

	_level = Label.new()
	_level.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_level.add_theme_font_size_override("font_size", 11)
	vbox.add_child(_level)

	_hunger = _make_bar(Color(0.95, 0.55, 0.15))
	_energy = _make_bar(Color(0.25, 0.65, 1.00))
	_xp     = _make_bar(Color(0.45, 0.85, 0.35))
	vbox.add_child(_hunger)
	vbox.add_child(_energy)
	vbox.add_child(_xp)

func _make_bar(fill_color: Color) -> ProgressBar:
	var bar := ProgressBar.new()
	bar.min_value = 0.0
	bar.max_value = 100.0
	bar.show_percentage = false
	bar.custom_minimum_size = Vector2(0, 10)
	var fill := StyleBoxFlat.new()
	fill.bg_color = fill_color
	bar.add_theme_stylebox_override("fill", fill)
	var bg := StyleBoxFlat.new()
	bg.bg_color = Color(0.12, 0.12, 0.12)
	bar.add_theme_stylebox_override("background", bg)
	return bar

# Mostra o balão por N segundos, mesmo sem hover (usado por "Ver status").
func show_status(seconds: float) -> void:
	_until_msec = Time.get_ticks_msec() + int(seconds * 1000.0)

func _process(_delta: float) -> void:
	var pet := get_parent()
	visible = pet.hovered or Time.get_ticks_msec() < _until_msec
	_hunger.value = PetStats.hunger
	_energy.value = PetStats.energy
	_xp.value     = PetStats.xp
	_level.text   = "Nv.%d" % PetStats.level
```

- [ ] **Step 2: Colocar o balão no `Pokemon.tscn`**

Em `godot/scenes/Pokemon.tscn`:
- mudar `load_steps=4` para `load_steps=5`;
- adicionar após a linha `[ext_resource type="Script" ... id="1"]`:

```
[ext_resource type="Script" path="res://scripts/balloon.gd" id="2"]
```

- adicionar ao final:

```
[node name="Balloon" type="PanelContainer" parent="."]
script = ExtResource("2")
```

- [ ] **Step 3: Remover o HUD fixo**

Em `godot/scenes/World.tscn`:
- mudar `load_steps=5` de volta para `load_steps=4`;
- remover a linha `[ext_resource type="Script" path="res://scripts/hud.gd" id="4"]`;
- remover o bloco:

```
[node name="HUD" type="CanvasLayer" parent="."]
script = ExtResource("4")
```

Excluir os arquivos:

```bash
git rm godot/scripts/hud.gd godot/scripts/sprite_loader.gd
```

- [ ] **Step 4: Verificação manual**

Run: dar play. Esperado: sem HUD fixo no rodapé; ao passar o cursor sobre o
bicho, balão sobe com nível e barras de fome/energia/XP; some quando o cursor
sai. Nenhum erro de script ausente no console.

- [ ] **Step 5: Commit**

```bash
git add godot/scripts/balloon.gd godot/scenes/Pokemon.tscn godot/scenes/World.tscn
git commit -m "feat(godot): balão de HUD sob o mouse, remove HUD fixo e loader legado

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 9: Shell Windows — escala, F2, menu completo, save nos eventos

**Files:**
- Modify: `godot/scripts/app_window.gd` (reescrever)

Sem testes automatizados (precisa de janela/OS); verificação manual no passo 4.

- [ ] **Step 1: Reescrever `app_window.gd`**

Substituir todo o conteúdo de `godot/scripts/app_window.gd` por:

```gdscript
extends Node
# Autoload: AppWindow — dono da janela do desktop pet: sem borda, transparente,
# sempre no topo; arraste (fora do bicho); F2 esconde/mostra; escala; menu de
# contexto; save nos eventos. A bandeja entra na Tarefa 10. Não conhece mecânica.

const BASE_SIZE := Vector2i(480, 480)

var _dragging := false
var _drag_start: Vector2 = Vector2.ZERO
var _hidden := false
var _tray: Object = null  # instância TrayIcon quando a GDExtension existir

func _ready() -> void:
	var win := get_window()
	win.borderless    = true
	win.always_on_top = true
	win.transparent   = true
	win.title         = "TamaPokeWin"
	win.close_requested.connect(quit_pet)  # Alt+F4: salva e sai
	_setup_tray()

func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo \
			and event.keycode == KEY_F2:
		_toggle_window()
	elif event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_RIGHT and mb.pressed:
			_show_context_menu(mb.global_position)
		elif mb.button_index == MOUSE_BUTTON_LEFT:
			if mb.pressed and not _pet_hovered():
				_dragging   = true
				_drag_start = DisplayServer.mouse_get_position()
			elif not mb.pressed:
				_dragging = false
	elif event is InputEventMouseMotion and _dragging:
		var delta := DisplayServer.mouse_get_position() - _drag_start
		if delta.length() > 4.0:  # threshold: clique não vira arrasto
			_drag_start = DisplayServer.mouse_get_position()
			get_window().position += Vector2i(delta)

func _pet_hovered() -> bool:
	var pet := get_tree().get_first_node_in_group("pet")
	return pet != null and pet.hovered

# ── Menu de contexto ─────────────────────────────────────────────────

func _show_context_menu(pos: Vector2) -> void:
	var menu := PopupMenu.new()
	var esc := PopupMenu.new()
	esc.name = "escala"
	esc.add_item("0,5×", 5)
	esc.add_item("1×", 10)
	esc.add_item("2×", 20)
	esc.id_pressed.connect(_on_scale_selected)
	menu.add_child(esc)
	menu.add_submenu_item("Escala", "escala")
	menu.add_check_item("Sempre no topo", 1)
	menu.set_item_checked(1, get_window().always_on_top)
	menu.add_item("Ver status", 2)
	menu.add_separator()
	menu.add_item("Sair", 3)
	menu.id_pressed.connect(_on_menu_item.bind(menu))
	add_child(menu)
	menu.popup(Rect2i(Vector2i(pos), Vector2i.ZERO))

func _on_scale_selected(id: int) -> void:
	set_window_scale(id / 10.0)

func _on_menu_item(id: int, menu: PopupMenu) -> void:
	menu.queue_free()
	match id:
		1:
			var win := get_window()
			win.always_on_top = not win.always_on_top
		2:
			_show_status()
		3:
			quit_pet()

# ── Ações da janela ──────────────────────────────────────────────────

# Escala 0,5×/1×/2×: muda o tamanho da janela; o stretch canvas_items do
# projeto amplia o mesmo recorte (o bicho fica maior, não aparece mais mapa).
func set_window_scale(k: float) -> void:
	get_window().size = Vector2i(BASE_SIZE * k)

# F2 / bandeja: esconde ou mostra a janela (o bicho continua vivo escondido).
func _toggle_window() -> void:
	_hidden = not _hidden
	get_window().visible = not _hidden
	if _hidden:
		PetStats.save_data()  # salvamento por segurança ao esconder

# "Ver status" (menu/bandeja): mostra a janela e aciona o balão por 5 s.
func _show_status() -> void:
	if _hidden:
		_toggle_window()
	var pet := get_tree().get_first_node_in_group("pet")
	if pet:
		var balloon := pet.get_node_or_null("Balloon")
		if balloon:
			balloon.show_status(5.0)

func quit_pet() -> void:
	PetStats.save_data()
	get_tree().quit()

# ── Bandeja (Tarefa 10; noop quando a extensão não está instalada) ────

func _setup_tray() -> void:
	if not ClassDB.class_exists("TrayIcon"):
		return  # sem GDExtension (CI/WSL): roda sem bandeja
	_tray = ClassDB.instantiate("TrayIcon")
	_tray.show_icon(ProjectSettings.globalize_path("res://assets/tamapoke.ico"), "TamaPoke")
	_tray.connect("toggle_pressed", _toggle_window)
	_tray.connect("status_pressed", _show_status)
	_tray.connect("quit_pressed", quit_pet)
```

- [ ] **Step 2: Verificação manual**

Run: dar play. Esperado:
- Arrastar com o botão esquerdo fora do bicho move a janela; arrastar sobre o
  bicho não move (ele dá carinho em vez disso).
- Clique direito abre o menu; Escala 0,5×/2× muda o tamanho da janela e o bicho
  fica proporcionalmente maior/menor; "Sempre no topo" alterna o check.
- F2 esconde a janela (o processo continua vivo). Antes da bandeja existir
  (Task 10), reabrir é fechando o app e rodando de novo.
- "Ver status" faz o balão aparecer por ~5 s.
- Fechar por Alt+F4 salva (conferir que `user://pet_save.cfg` foi atualizado).

- [ ] **Step 3: Commit**

```bash
git add godot/scripts/app_window.gd
git commit -m "feat(godot): shell Windows — escala, F2, menu completo, save nos eventos

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 10: Bandeja do Windows (GDExtension)

**Files:**
- Create: `godot/src/tray/src/tray_extension.cpp`
- Create: `godot/src/tray/SConstruct`
- Create: `godot/src/tray/tamapoke_tray.gdextension`
- Create: `godot/assets/tamapoke.ico` (gerado a partir do ícone do repo)
- Modify: `.gitignore` (godot-cpp e bin da extensão)
- Modify: `godot/scripts/app_window.gd` (já preparado na Task 9 — `_setup_tray`)

- [ ] **Step 1: Gerar o ícone .ico e ignorar o build nativo**

```bash
python -I tools/make_icon.py          # gera assets/tamapoke.ico (raiz do repo)
cp assets/tamapoke.ico godot/assets/tamapoke.ico
printf "\ngodot/src/tray/godot-cpp/\ngodot/src/tray/bin/\n" >> .gitignore
```

- [ ] **Step 2: Escrever a extensão**

`godot/src/tray/src/tray_extension.cpp`:

```cpp
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>

#include <windows.h>
#include <shellapi.h>

using namespace godot;

// Ícone na bandeja do Windows (Shell_NotifyIconW) com menu nativo.
// Não sabe nada do jogo: emite sinais e o AppWindow decide o que fazer.
// Sobrescreve a window proc do Godot para receber os cliques da bandeja.

class TrayIcon : public Object {
	GDCLASS(TrayIcon, Object)

private:
	static TrayIcon *singleton;

	NOTIFYICONDATAW nid = {};
	bool visible = false;
	HWND hwnd = nullptr;
	HMENU menu = nullptr;
	WNDPROC prev_proc = nullptr;
	UINT wm_taskbar_created = 0;

	enum { IDM_TOGGLE = 1, IDM_STATUS = 2, IDM_QUIT = 3 };
	enum { TRAY_ID = 7, TRAY_MSG = WM_USER + 7 };

	LRESULT _handle(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
		if (msg == wm_taskbar_created && visible) {
			Shell_NotifyIconW(NIM_ADD, &nid); // explorer reiniciou: recoloca
		}
		if (msg == TRAY_MSG && (lp == WM_LBUTTONUP || lp == WM_RBUTTONUP)) {
			_popup_menu(h);
		}
		if (msg == WM_COMMAND) {
			switch (LOWORD(wp)) {
				case IDM_TOGGLE: emit_signal("toggle_pressed"); break;
				case IDM_STATUS: emit_signal("status_pressed"); break;
				case IDM_QUIT:   emit_signal("quit_pressed");   break;
			}
		}
		return CallWindowProc(prev_proc, h, msg, wp, lp);
	}

	void _popup_menu(HWND h) {
		POINT pt;
		GetCursorPos(&pt);
		SetForegroundWindow(h); // exigido para o menu fechar ao clicar fora
		TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
		PostMessage(h, WM_NULL, 0, 0);
	}

	static LRESULT CALLBACK _proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
		if (singleton != nullptr) {
			return singleton->_handle(h, msg, wp, lp);
		}
		return DefWindowProc(h, msg, wp, lp);
	}

public:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("show_icon", "icon_path", "tooltip"), &TrayIcon::show_icon);
		ClassDB::bind_method(D_METHOD("hide_icon"), &TrayIcon::hide_icon);
		ClassDB::add_signal("TrayIcon", MethodInfo("toggle_pressed"));
		ClassDB::add_signal("TrayIcon", MethodInfo("status_pressed"));
		ClassDB::add_signal("TrayIcon", MethodInfo("quit_pressed"));
	}

	TrayIcon() { singleton = this; }
	~TrayIcon() { hide_icon(); singleton = nullptr; }

	void show_icon(const String &icon_path, const String &tooltip) {
		if (visible) {
			return;
		}
		hwnd = (HWND)(intptr_t)DisplayServer::get_singleton()->window_get_window_handle(0);

		memset(&nid, 0, sizeof(nid));
		nid.cbSize           = sizeof(nid);
		nid.hWnd             = hwnd;
		nid.uID              = TRAY_ID;
		nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		nid.uCallbackMessage = TRAY_MSG;
		nid.hIcon            = (HICON)LoadImageW(nullptr, icon_path.wide_string().get_data(),
				IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
		wcsncpy_s(nid.szTip, tooltip.wide_string().get_data(), _TRUNCATE);

		if (nid.hIcon == nullptr) {
			ERR_PRINT("TrayIcon: não conseguiu carregar o ícone");
		}
		if (menu == nullptr) {
			menu = CreatePopupMenu();
			AppendMenuW(menu, MF_STRING, IDM_TOGGLE, L"Mostrar/Esconder");
			AppendMenuW(menu, MF_STRING, IDM_STATUS, L"Ver status");
			AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
			AppendMenuW(menu, MF_STRING, IDM_QUIT, L"Sair");
		}

		wm_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
		if (prev_proc == nullptr) {
			prev_proc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)_proc);
		}

		visible = Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
		if (!visible) {
			ERR_PRINT("TrayIcon: Shell_NotifyIconW(NIM_ADD) falhou");
		}
	}

	void hide_icon() {
		if (visible && hwnd != nullptr) {
			Shell_NotifyIconW(NIM_DELETE, &nid);
			if (nid.hIcon != nullptr) {
				DestroyIcon(nid.hIcon);
			}
		}
		if (prev_proc != nullptr) {
			SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)prev_proc);
			prev_proc = nullptr;
		}
		if (menu != nullptr) {
			DestroyMenu(menu);
			menu = nullptr;
		}
		visible = false;
	}
};

TrayIcon *TrayIcon::singleton = nullptr;

static void initialize_tray_module(godot::ModuleInitializationLevel p_level) {
	if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	godot::ClassDB::register_class<TrayIcon>();
}

extern "C" GDExtensionBool GDE_EXPORT tamapoke_tray_library_init(
		GDExtensionInterfaceGetProcAddress p_get,
		const GDExtensionClassLibraryPtr p_lib,
		GDExtensionInitialization *r_init) {
	godot::GDExtensionBinding::InitObject init_obj(p_get, p_lib, r_init);
	init_obj.register_initializer(initialize_tray_module);
	init_obj.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
	return init_obj.init();
}
```

`godot/src/tray/SConstruct`:

```python
#!/usr/bin/env python
# Build da GDExtension (no Windows, com MSVC e SCons instalados):
#   git clone --branch master https://github.com/godotengine/godot-cpp godot-cpp
#   scons platform=windows arch=x86_64
# Saída: bin/tamapoke_tray.dll

env = SConscript("godot-cpp/SConstruct")
env.Append(CPPPATH=["src/"])
if env["platform"] == "windows":
    env.Append(LIBS=["shell32"])

env.SharedLibrary(target="bin/tamapoke_tray", source=["src/tray_extension.cpp"])
```

`godot/src/tray/tamapoke_tray.gdextension`:

```
[configuration]
entry_symbol = "tamapoke_tray_library_init"
compatibility_minimum = "4.7"

[libraries]
windows.debug.x86_64           = "res://src/tray/bin/tamapoke_tray.dll"
windows.release.x86_64         = "res://src/tray/bin/tamapoke_tray.dll"
windows.template_debug.x86_64  = "res://src/tray/bin/tamapoke_tray.dll"
windows.template_release.x86_64 = "res://src/tray/bin/tamapoke_tray.dll"
```

- [ ] **Step 3: Build no Windows (não roda no WSL)**

No PowerShell:

```powershell
cd godot\src\tray
git clone --branch master https://github.com/godotengine/godot-cpp godot-cpp
scons platform=windows arch=x86_64
# Esperado: bin\tamapoke_tray.dll criado
```

(Se o clone da tag 4.7 não existir, usar `--branch master` e conferir a
compatibilidade de API no header do godot-cpp.)

- [ ] **Step 4: Verificação manual**

Run: dar play no projeto (Windows). Esperado:
- Ícone do TamaPoke na bandeja; hover mostra o tooltip "TamaPoke".
- Menu da bandeja: "Mostrar/Esconder" liga/desliga a janela (F2 equivalente),
  "Ver status" mostra a janela + balão por 5 s, "Sair" salva e encerra.
- Matar o explorer.exe e reiniciá-lo recoloca o ícone (message TaskbarCreated).
- Fechar o jogo remove o ícone da bandeja.

- [ ] **Step 5: Commit**

```bash
git add godot/src/tray/src/tray_extension.cpp godot/src/tray/SConstruct godot/src/tray/tamapoke_tray.gdextension godot/assets/tamapoke.ico .gitignore
git commit -m "feat(godot): ícone na bandeja via GDExtension (Shell_NotifyIcon)

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

### Task 11: Verificação final e afinação do jardim

**Files:**
- Modify (se necessário): `godot/scenes/World.tscn` (retângulos navegáveis, pontos de vagância, posições das zonas)

- [ ] **Step 1: Suíte headless verde**

Run (em clone fresco, primeiro `--import` para gerar o cache de classes globais
de `PmdSpriteLoader`/`DayNight`, sem o qual os testes não compilam):

```bash
godot --headless --path godot --import
godot --headless --path godot --script res://tests/run_all.gd
```
Expected: `OK — todos os testes passaram`; exit 0.

- [ ] **Step 2: Afinação visual do jardim (com o jogo aberto, aba Remote)**

Conferir com o bicho vagando (esticar o tempo de jogo ou setar no Remote
`PetStats.hunger = 10.0` / `PetStats.energy = 10.0` para forçar as buscas):

- O bicho não atravessa água nem passa por dentro de árvores → se passar, encolher
  os `walkable_rects` no `World.tscn` e testar de novo.
- As pontes dão passagem (bicho cruza as duas margens) → se não cruza, alargar os
  retângulos das pontes para sobrepor as margens.
- O bicho "encaixa" nas três zonas ao chegar (animação certa) → ajustar as
  posições `EatZone`/`TrainZone`/`SleepZone` se estiverem deslocadas dos props
  (caixas de frutas, alvo do dojo, porta da casinha).

Commit (só se algo mudou):

```bash
git add godot/scenes/World.tscn
git commit -m "fix(godot): afina navegação e zonas do jardim

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

- [ ] **Step 3: Checklist de comportamento (spec, seção 7)**

Marcar cada item, com o jogo rodando e a aba Remote quando precisar forçar stats:

1. Vagar desvia de água/árvores e às vezes cruza as pontes.
2. `hunger = 10.0` → vai até as frutas, come (Eat), fome volta a 100.
3. `energy = 10.0` → vai à casinha, dorme (Sleep), energia sobe ~8/s.
4. Treino: bicho alterna vagar/descansar; ao cruzar o dojo acumula XP (balão).
5. Carinho: clique = pulso + corações; +2 XP; segundo clique em <30 s não soma.
6. `hour_override = 23.0` → tint escurece e o bicho vai para a cama; acorda
   com `hour_override = 12.0`; com fome < 30 à noite acorda, come, volta.
7. Evolução: Remote `PetStats.add_xp(99999.0, "cheat")` → sobe níveis; no 17
   vira Charmeleon (sprite recarregado), no 37 Charizard (sem Eat, usa Idle).
8. Escala 0,5×/1×/2× pelo menu; bicho proporcional; sempre no topo liga/desliga.
9. F2 esconde/mostra; bandeja: Mostrar/Esconder, Ver status, Sair.
10. Save: fechar (Sair) e reabrir → estado mantido; com save antigo, o bicho
    aparece alimentado e com o XP offline (testar mudando a data do PC +1 h
    antes de reabrir: no mínimo fome/energia consistentes e XP +1..12).

Qualquer item falho: corrigir e re-testar antes de marcar.

- [ ] **Step 4: Commit final da verificação**

```bash
git commit --allow-empty -m "test(godot): verificação manual do MVP completa

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```

---

## Auto-revisão do plano (feita pelo autor)

- **Cobertura do spec:** mundo top-down (Tasks 1, 3), navegação completa com
  pontes (Tasks 1, 3), zonas nos marcos do mapa (Tasks 1, 11), 8 direções
  (Task 2), stats em tempo real (Task 4), sim offline fiel (Task 4), dia/noite
  com rotina (Tasks 5, 6), carinho (Task 7), balão sob o mouse (Task 8), shell
  completo (Task 9), bandeja GDExtension (Task 10), verificação (Task 11).
  Tudo que o spec lista em "Dentro" tem tarefa; "Fora" (áudio, outras espécies,
  ovo, autostart) não tem tarefa — correto.
- **Consistência de nomes:** `PetStats.tick/asleep/try_pet/is_night/simulate_offline/
  local_hour_now/hour_override/save_path`, `PmdSpriteLoader.direction_for_velocity`,
  animações `Walk0..Walk7`, `world.random_wander_point()`, `balloon.show_status(s)`,
  `hovered` no controller, sinais `toggle_pressed/status_pressed/quit_pressed` —
  usados e definidos nos mesmos nomes ao longo do plano.
- **Sem placeholders:** todos os passos têm código completo ou comando exato;
  as únicas "afinações" explícitas (coordenadas do jardim) têm valores iniciais
  concretos e um procedimento de ajuste na Task 11.
