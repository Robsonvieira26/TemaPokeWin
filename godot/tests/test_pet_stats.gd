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
