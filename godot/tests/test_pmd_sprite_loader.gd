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
