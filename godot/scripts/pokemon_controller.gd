extends CharacterBody2D

# Controlador autônomo do pokémon — máquina de estados + animações PMD Collab.
# Sprites: PMD Collab (sprites.pmdcollab.org), carregados por PmdSpriteLoader.
# Espécie determinada pelo nível do pet: Charmander → Charmeleon → Charizard.

enum State { WANDER, IDLE, ACTIVITY, SEEK }

const WALK_SPEED    := 60.0
const GROUND_Y      := 264.0
const WORLD_LEFT    := 32.0
const WORLD_RIGHT   := 1888.0
const HUNGER_LOW    := 30.0   # vai buscar comida abaixo deste valor
const ENERGY_LOW    := 20.0   # vai dormir abaixo deste valor

# Mapeamento nível → espécie PMD Collab
const LEVEL_THRESHOLDS := { 37: "0006", 17: "0005", 1: "0004" }

var state:        State  = State.WANDER
var direction:    int    = 1
var state_timer:  float  = 0.0
var active_zone:  Node   = null
var _species_id:  String = "0004"
var _last_level:  int    = 1
var _seek_target: Vector2 = Vector2.ZERO

@onready var camera: Camera2D         = $"../Camera"
@onready var sprite: AnimatedSprite2D = $Visual

func _ready() -> void:
	position = Vector2(480.0, GROUND_Y)
	_load_sprites()
	_set_state(State.WANDER)

func _physics_process(delta: float) -> void:
	state_timer -= delta

	# recarrega sprites se evoluiu
	if PetStats.level != _last_level:
		_last_level = PetStats.level
		_load_sprites()

	match state:
		State.WANDER:   _do_wander(delta)
		State.IDLE:     _do_idle()
		State.ACTIVITY: _do_activity(delta)
		State.SEEK:     _do_seek(delta)

	if camera:
		camera.position.x = position.x

func _do_wander(delta: float) -> void:
	position.x += direction * WALK_SPEED * delta
	position.y  = GROUND_Y
	position.x  = clampf(position.x, WORLD_LEFT, WORLD_RIGHT)

	if position.x <= WORLD_LEFT or position.x >= WORLD_RIGHT:
		direction *= -1
		_update_sprite_flip()

	if state_timer <= 0.0:
		var need := _check_need()
		if need != "":
			_start_seek(need)
		else:
			_set_state(State.IDLE)

func _do_idle() -> void:
	position.y = GROUND_Y
	if state_timer <= 0.0:
		var need := _check_need()
		if need != "":
			_start_seek(need)
		else:
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

func _do_seek(delta: float) -> void:
	position.y = GROUND_Y
	var dx := _seek_target.x - position.x
	if absf(dx) < 8.0:
		# chegou perto o suficiente; a ActivityZone dispara body_entered
		_set_state(State.IDLE)
		return
	direction = 1 if dx > 0 else -1
	position.x += direction * WALK_SPEED * delta
	position.x  = clampf(position.x, WORLD_LEFT, WORLD_RIGHT)
	_update_sprite_flip()
	if state_timer <= 0.0:
		_set_state(State.WANDER)  # desistiu

func _check_need() -> String:
	if PetStats.hunger < HUNGER_LOW:
		return "eat"
	if PetStats.energy < ENERGY_LOW:
		return "sleep"
	return ""

func _start_seek(zone_name: String) -> void:
	for zone: Node in get_tree().get_nodes_in_group("activity_zones"):
		if zone.get("activity_name") == zone_name:
			_seek_target = (zone as Node2D).global_position
			state      = State.SEEK
			state_timer = 20.0
			_play_anim("Walk")
			return
	_set_state(State.WANDER)  # zona não encontrada, continua vagando

func _set_state(new_state: State) -> void:
	state = new_state
	match new_state:
		State.WANDER:
			state_timer = randf_range(4.0, 9.0)
			_play_anim("Walk")
		State.IDLE:
			state_timer = randf_range(1.5, 4.0)
			_play_anim("Idle")
		State.ACTIVITY:
			state_timer = randf_range(4.0, 10.0)
			# animação de atividade definida em enter_activity()
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

func startle() -> void:
	_set_state(State.IDLE)
	state_timer = 2.0

# ── Sprites ──────────────────────────────────────────────────────────

func _load_sprites() -> void:
	var new_species := _species_for_level(PetStats.level)
	if new_species == _species_id and sprite != null and sprite.sprite_frames != null:
		return
	_species_id = new_species
	var frames := PmdSpriteLoader.load_pokemon_frames(_species_id)
	if frames and sprite:
		sprite.sprite_frames = frames

func _play_anim(anim_name: String) -> void:
	if sprite == null or sprite.sprite_frames == null:
		return
	# Fallback: se animação não existe (ex: Charizard sem Eat) usa Idle
	if not sprite.sprite_frames.has_animation(anim_name):
		anim_name = "Idle"
	if sprite.animation != anim_name:
		sprite.play(anim_name)
	_update_sprite_flip()

func _update_sprite_flip() -> void:
	if sprite:
		sprite.flip_h = (direction == -1)

static func _species_for_level(lvl: int) -> String:
	for threshold in [37, 17, 1]:
		if lvl >= threshold:
			return LEVEL_THRESHOLDS[threshold]
	return "0004"
