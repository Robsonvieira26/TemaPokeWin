extends "res://tests/test_base.gd"
# Testa a detecção de travamento do controller (unidade: amostragem de
# progresso, desvio e reset de contagem; sem quadros de física — os métodos
# são chamados direto). No harness headless o nó não entra na árvore durante
# _initialize (get_tree() == null), então _world() devolve null e os desvios
# que dependem do World ficam guardados — o comportamento real de jogo é
# exercitado pelos testes do World e na verificação manual.

const SPAWN := Vector2(980, 545)

func _fresh() -> CharacterBody2D:
	# Cena (não script .new()): o controller precisa de $NavAgent/$Visual/$PetArea.
	var c: CharacterBody2D = load("res://scenes/Pokemon.tscn").instantiate()
	c.position = SPAWN
	return c

func test_reroute_em_seek_abandona_busca_e_desvia() -> void:
	var c := _fresh()
	c.state = c.State.SEEK
	c._stuck_for = c.STUCK_SECONDS
	c._reroute()
	assert_eq(c.state, c.State.WANDER, "preso buscando zona → desvia para passeio")
	assert_almost(c._stuck_for, 0.0, 0.001, "contador de travamento zerado no desvio")
	c.free()

func test_reroute_em_wander_mantem_estado_e_zera_contagem() -> void:
	var c := _fresh()
	c.state = c.State.WANDER
	c._stuck_for = c.STUCK_SECONDS
	c._reroute()
	assert_eq(c.state, c.State.WANDER, "preso no passeio continua passeando")
	assert_almost(c._stuck_for, 0.0, 0.001, "contador de travamento zerado no desvio")
	c.free()

func test_track_stuck_conta_amostas_parado_e_desvia() -> void:
	var c := _fresh()
	c._stuck_pos = c.position
	for i in 9:  # 4,5 s parado: ainda não desvia
		c._track_stuck(0.5)
	assert_almost(c._stuck_for, 4.5, 0.001, "4,5 s parado acumulam sem desviar")
	c._track_stuck(0.5)  # 5,0 s → _reroute dispara
	assert_almost(c._stuck_for, 0.0, 0.001, "desvio zera o contador")
	c.free()

func test_progresso_zera_o_contador() -> void:
	var c := _fresh()
	c._stuck_pos = c.position
	c._track_stuck(0.5)
	assert_almost(c._stuck_for, 0.5, 0.001, "amostra parada acumula")
	c.position += Vector2(30, 0)  # ~0,5 s a WALK_SPEED
	c._track_stuck(0.5)
	assert_almost(c._stuck_for, 0.0, 0.001, "andar 30 px na amostra zera o contador")
	c.free()

func test_mudanca_de_estado_zera_contador() -> void:
	var c := _fresh()
	c._stuck_for = 3.0
	c._set_state(c.State.IDLE)
	assert_almost(c._stuck_for, 0.0, 0.001, "estado novo recomeça a contagem")
	c.free()