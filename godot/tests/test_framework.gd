extends "res://tests/test_base.gd"

func test_framework_reports_pass() -> void:
	assert_true(true, "sempre verdadeiro")

func test_framework_detects_wrong_eq() -> void:
	# auto-teste: duas afirmações válidas e uma contagem manual
	assert_eq(2 + 2, 4, "aritmética")
	assert_almost(0.1 + 0.2, 0.3, 0.0001, "float")
