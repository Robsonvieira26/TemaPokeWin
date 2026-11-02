extends CharacterBody2D

# Controlador autônomo do pokémon — máquina de estados simples.
# Não depende de sprites: usa um ColorRect como placeholder.

enum State { WANDER, IDLE, ACTIVITY }

const WALK_SPEED  := 60.0   # px/s
const GROUND_Y    := 264.0  # y fixo na faixa de grama (≈55% de 480)
const WORLD_LEFT  := 32.0
const WORLD_RIGHT := 1888.0 # largura do mundo − margem

var state:       State  = State.WANDER
var direction:   int    = 1     # 1 = direita, -1 = esquerda
var state_timer: float  = 0.0
var active_zone: Node   = null

@onready var camera: Camera2D = $"../Camera"

func _ready() -> void:
	position = Vector2(480.0, GROUND_Y)
	_set_state(State.WANDER)

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
		State.IDLE:
			state_timer = randf_range(1.5, 4.0)
		State.ACTIVITY:
			state_timer = randf_range(4.0, 10.0)

# Chamado pelo activity_zone.gd
func enter_activity(zone: Node) -> void:
	active_zone = zone
	_set_state(State.ACTIVITY)

func exit_activity(_zone: Node) -> void:
	if active_zone == _zone:
		active_zone = null
		_set_state(State.WANDER)

# Interação do usuário: clique pausa o pokémon brevemente
func startle() -> void:
	_set_state(State.IDLE)
	state_timer = 2.0
