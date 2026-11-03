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

func test_tint_boundaries_are_continuous() -> void:
	assert_almost(DayNight.tint_for_hour(19.999).g, DayNight.tint_for_hour(20.0).g, 0.01, "fim do entardecer ≈ início do anoitecer")
	assert_almost(DayNight.tint_for_hour(21.999).g, DayNight.tint_for_hour(22.0).g, 0.01, "fim do anoitecer ≈ noite")
