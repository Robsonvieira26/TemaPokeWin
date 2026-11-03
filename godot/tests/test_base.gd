extends RefCounted
# Base para suites de teste. Cada método test_* roda uma vez; asserts coletam falhas.

var _failures: PackedStringArray = []
var _method := ""
var _completed := PackedStringArray()  # testes que terminaram sem abortar

# Um teste que aborta (erro de runtime) não executa o append abaixo;
# run() percebe a ausência do nome em _completed e conta como falha.
func _call_test(m_name: String) -> void:
	call(m_name)
	_completed.append(m_name)

func run() -> int:
	var suite: String = get_script().resource_path.get_file()
	var failed := 0
	_completed = PackedStringArray()
	for m in get_method_list():
		if String(m.name).begins_with("test_"):
			var before := _failures.size()
			_method = String(m.name)
			_call_test(_method)
			if not _completed.has(_method):
				failed += 1
				printerr("FAIL  %s.%s" % [suite, _method])
				printerr("      abortou com erro de runtime")
				continue
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
