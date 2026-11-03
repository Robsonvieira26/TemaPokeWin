extends CharacterBody2D
# Comportamento do bicho: FSM autônoma + navegação pelo jardim + 8 direções.
# Sprites: PMD Collab via PmdSpriteLoader; espécie pelo nível (1/17/37).
# As necessidades se resolvem sozinhas: fome baixa → tigela; energia baixa → cama.

enum State { WANDER, IDLE, ACTIVITY, SEEK }

const WALK_SPEED := 30.0  # desktop pet: passinho tranquilo (60 era frenético)
const HUNGER_LOW := 30.0   # fome abaixo disso → busca comida
const ENERGY_LOW := 20.0   # energia abaixo disso → busca a cama
const STUCK_SECONDS := 5.0  # preso tentando chegar a um lugar → faz outro caminho

var state:       State = State.WANDER
var state_timer: float = 0.0
var active_zone: Node  = null
var _seek_zone:  Node  = null
var _stuck_for:   float   = 0.0    # segundos sem progresso rumo ao destino
var _stuck_check: float    = 0.0   # relógio da amostragem de progresso
var _stuck_pos:   Vector2  = Vector2.ZERO
var _facing:     int   = PmdSpriteLoader.DIR_SOUTH
var _species_id: String = "0004"
var _last_level: int    = 1
var hovered := false  # cursor sobre o bicho (usado pelo balão e pelo arrasto da janela)

@onready var nav: NavigationAgent2D = $NavAgent
@onready var sprite: AnimatedSprite2D = $Visual

func _ready() -> void:
	add_to_group("pet")
	$PetArea.input_event.connect(_on_pet_input)
	$PetArea.mouse_entered.connect(func(): hovered = true)
	$PetArea.mouse_exited.connect(func(): hovered = false)
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
	# get_tree() pode ser null fora da árvore (teardown ou harness headless).
	var tree := get_tree()
	if tree == null:
		return null
	return tree.get_first_node_in_group("world")

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
		if active_zone.activity_name == "sleep" \
				and PetStats.is_night(PetStats.local_hour_now()) \
				and PetStats.hunger >= HUNGER_LOW:
			state_timer = maxf(state_timer, 1.0)  # segura na cama
			PetStats.asleep = true
	if state_timer <= 0.0:
		PetStats.asleep = false
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
	_track_stuck(get_physics_process_delta_time())
	return false

# Progresso rumo ao destino, amostrado a cada 0,5 s (a WALK_SPEED, andar
# desloca ~15 px por amostra; ficar preso, quase nada). 5 s parados → desvia.
func _track_stuck(delta: float) -> void:
	_stuck_check -= delta
	if _stuck_check > 0.0:
		return
	_stuck_check = 0.5
	if global_position.distance_to(_stuck_pos) < 4.0:
		_stuck_for += 0.5
		if _stuck_for >= STUCK_SECONDS:
			_reroute()
	else:
		_stuck_for = 0.0
	_stuck_pos = global_position

# "Fazer outro caminho": em busca de zona, abandona o destino e passa a andar
# até um ponto aleatório (o IDLE retoma a busca se a necessidade persistir);
# no passeio, escolhe outro ponto.
func _reroute() -> void:
	_stuck_for   = 0.0
	_stuck_check = 0.0
	_stuck_pos   = global_position
	if state == State.SEEK:
		_set_state(State.WANDER)
	else:
		var w := _world()
		if w:
			nav.target_position = w.random_wander_point()

func _check_need() -> String:
	if PetStats.hunger < HUNGER_LOW:
		return "eat"
	if PetStats.energy < ENERGY_LOW or PetStats.is_night(PetStats.local_hour_now()):
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
	PetStats.asleep = false
	_stuck_for = 0.0  # destino novo, contagem de travamento zerada
	_stuck_pos = global_position
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
