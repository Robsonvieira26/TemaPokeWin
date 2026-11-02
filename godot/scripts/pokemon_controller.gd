extends CharacterBody2D

# Controlador autônomo do pokémon — máquina de estados simples.
# Sprites PMDCollab (CC BY-NC 4.0): Walk, Idle, Sleep, Eat, Charge → train
# Opção futura: usar pokesprite + pipeline de conversão para mais assets animados.

enum State { WANDER, IDLE, ACTIVITY }

const WALK_SPEED  := 60.0   # px/s
const GROUND_Y    := 264.0  # y fixo na faixa de grama (≈55% de 480)
const WORLD_LEFT  := 32.0
const WORLD_RIGHT := 1888.0 # largura do mundo − margem

var state:       State  = State.WANDER
var direction:   int    = 1     # 1 = direita, -1 = esquerda
var state_timer: float  = 0.0
var active_zone: Node   = null

@onready var camera:         Camera2D        = $"../Camera"
@onready var anim_sprite:    AnimatedSprite2D = $AnimatedSprite2D

func _ready() -> void:
	position = Vector2(480.0, GROUND_Y)
	_setup_sprites()
	_set_state(State.WANDER)

func _setup_sprites() -> void:
	if anim_sprite == null:
		return
	var loader := preload("res://scripts/sprite_loader.gd").new()
	anim_sprite.sprite_frames = loader.build_sprite_frames()
	loader.free()

func _physics_process(delta: float) -> void:
	state_timer -= delta

	match state:
		State.WANDER:
			_do_wander(delta)
		State.IDLE:
			_do_idle()
		State.ACTIVITY:
			_do_activity(delta)

	# câmera suave seguindo o pokémon no eixo X
	if camera:
		camera.position.x = position.x

	# espelha o sprite conforme direção
	if anim_sprite:
		anim_sprite.flip_h = direction < 0

func _do_wander(delta: float) -> void:
	position.x += direction * WALK_SPEED * delta
	position.y  = GROUND_Y
	position.x  = clampf(position.x, WORLD_LEFT, WORLD_RIGHT)

	if position.x <= WORLD_LEFT or position.x >= WORLD_RIGHT:
		direction *= -1

	if state_timer <= 0.0:
		_set_state(State.IDLE)

func _do_idle() -> void:
	position.y = GROUND_Y
	if state_timer <= 0.0:
		direction = [-1, 1].pick_random()
		_set_state(State.WANDER)

func _do_activity(delta: float) -> void:
	position.y = GROUND_Y
	if active_zone:
		PetStats.add_xp(active_zone.xp_per_second * delta, active_zone.activity_name)
		match active_zone.stat_restore:
			"hunger": PetStats.restore_hunger(active_zone.restore_rate * delta)
			"energy": PetStats.restore_energy(active_zone.restore_rate * delta)
	if state_timer <= 0.0:
		_set_state(State.WANDER)

func _set_state(new_state: State) -> void:
	state = new_state
	match new_state:
		State.WANDER:
			state_timer = randf_range(4.0, 9.0)
			_play_anim("walk")
		State.IDLE:
			state_timer = randf_range(1.5, 4.0)
			_play_anim("idle")
		State.ACTIVITY:
			state_timer = randf_range(4.0, 10.0)
			# animação depende da zona; atualizado em enter_activity

func _play_anim(anim_name: String) -> void:
	if anim_sprite and anim_sprite.sprite_frames and \
			anim_sprite.sprite_frames.has_animation(anim_name):
		anim_sprite.play(anim_name)

# Chamado pelo activity_zone.gd
func enter_activity(zone: Node) -> void:
	active_zone = zone
	_set_state(State.ACTIVITY)
	match zone.activity_name:
		"eat":   _play_anim("eat")
		"sleep": _play_anim("sleep")
		"train": _play_anim("train")
		_:       _play_anim("idle")

func exit_activity(_zone: Node) -> void:
	if active_zone == _zone:
		active_zone = null
		_set_state(State.WANDER)

# Interação do usuário: clique pausa o pokémon brevemente
func startle() -> void:
	_set_state(State.IDLE)
	state_timer = 2.0
