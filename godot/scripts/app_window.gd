extends Node

# Controla o modo da janela: borderless, always-on-top, arraste pelo desktop.
# Autoload: AppWindow

var _drag_start: Vector2 = Vector2.ZERO
var _dragging: bool = false

func _ready() -> void:
	var win := get_window()
	win.borderless    = true
	win.always_on_top = true
	win.transparent   = true
	win.title         = "TemaPokeWin"

func _input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_RIGHT and mb.pressed:
			_show_context_menu(mb.global_position)
		elif mb.button_index == MOUSE_BUTTON_LEFT:
			_dragging = mb.pressed
			if _dragging:
				_drag_start = DisplayServer.mouse_get_position()
	elif event is InputEventMouseMotion and _dragging:
		var delta := DisplayServer.mouse_get_position() - _drag_start
		_drag_start = DisplayServer.mouse_get_position()
		get_window().position += Vector2i(delta)

func _show_context_menu(pos: Vector2) -> void:
	var menu := PopupMenu.new()
	menu.add_item("Ver status", 0)
	menu.add_separator()
	menu.add_item("Fechar", 1)
	menu.id_pressed.connect(_on_menu_item.bind(menu))
	add_child(menu)
	menu.popup(Rect2i(Vector2i(pos), Vector2i.ZERO))

func _on_menu_item(id: int, menu: PopupMenu) -> void:
	menu.queue_free()
	match id:
		0:
			print("Nível: ", PetStats.level, " | XP: ", int(PetStats.xp),
				" | Fome: ", int(PetStats.hunger), " | Energia: ", int(PetStats.energy))
		1:
			PetStats.save_data()
			get_tree().quit()
