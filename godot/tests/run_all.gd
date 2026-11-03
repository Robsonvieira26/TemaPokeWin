# Runner: godot --headless --path godot --script res://tests/run_all.gd
extends SceneTree

func _initialize() -> void:
	var failed := 0
	for f in DirAccess.get_files_at("res://tests"):
		if f.begins_with("test_") and f.ends_with(".gd") and f != "test_base.gd":
			var inst = load("res://tests/" + f).new()
			failed += inst.run()
	if failed == 0:
		print("OK — todos os testes passaram")
		quit(0)
	else:
		printerr("%d teste(s) falharam" % failed)
		quit(1)
