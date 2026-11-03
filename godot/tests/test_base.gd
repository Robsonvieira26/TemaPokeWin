extends RefCounted
# Base para suites de teste. Cada método test_* roda uma vez; asserts coletam falhas.

var _failures: PackedStringArray = []
var _method := ""

func run() -> int:
	var suite: String = get_script().resource_path.get_file()
	var failed := 0
	for m in get_method_list():
		if String(m.name).begins_with("test_"):
			var before := _failures.size()
			_method = m.name
			call(m.name)
			if _failures.size() > before:
				failed += 1
				printerr("FAIL  %s.%s" % [suite, m.name])
				for i in range(before, _failures.size()):
					printerr("      " + _failures[i])
			else:
				print("PASS  %s.%s" % [suite, m.name])
	return failed

func _fail(msg: String) -> void:
	_failures.append("%s: %s" % [_method, msg])

func assert_true(cond: bool, msg: String) -> void:
	if not cond:
		_fail(msg)

func assert_eq(got, want, msg := "") -> void:
	if got != want:
		_fail("%s esperado=%s obtido=%s" % [msg, want, got])

func assert_almost(got: float, want: float, eps: float, msg := "") -> void:
	if absf(got - want) > eps:
		_fail("%s esperado≈%s obtido=%s" % [msg, want, got])
