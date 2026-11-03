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
	_play_anim("Walk%d" % _facing)
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
			_play_anim("Walk%d" % _facing)
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
	if state == State.SEEK and zone != _seek_zone: return
	active_zone = zone
	_set_state(State.ACTIVITY)
	match zone.activity_name:
		"eat":   _play_anim("Eat")
		"sleep": _play_anim("Sleep")
		_:       _play_anim("Idle")

func exit_activity(_zone: Node) -> void:
	if active_zone == _zone:
		active_zone = null
		if state != State.WANDER:
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
