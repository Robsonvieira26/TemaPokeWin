extends Node
# Autoload: AppWindow — dono da janela do desktop pet: sem borda, transparente,
# sempre no topo; arraste (fora do bicho); F2 esconde/mostra; escala; menu de
# contexto; save nos eventos. A bandeja entra na Tarefa 10. Não conhece mecânica.

const BASE_SIZE := Vector2i(480, 480)

var _dragging := false
var _drag_start := Vector2i.ZERO  # DisplayServer.mouse_get_position() é Vector2i
var _hidden := false
var _tray: Object = null  # instância TrayIcon quando a GDExtension existir

func _ready() -> void:
	var win := get_window()
	win.borderless    = true
	win.always_on_top = true
	win.transparent   = true
	win.title         = "TamaPokeWin"
	win.close_requested.connect(quit_pet)  # Alt+F4: salva e sai
	_setup_tray()

func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed and not event.echo \
			and event.keycode == KEY_F2:
		_toggle_window()
	elif event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_RIGHT and mb.pressed:
			_show_context_menu(mb.global_position)
		elif mb.button_index == MOUSE_BUTTON_WHEEL_UP and mb.pressed:
			_zoom_step(0.5)
		elif mb.button_index == MOUSE_BUTTON_WHEEL_DOWN and mb.pressed:
			_zoom_step(-0.5)
		elif mb.button_index == MOUSE_BUTTON_LEFT:
			if mb.pressed and not _pet_hovered():
				_dragging   = true
				_drag_start = DisplayServer.mouse_get_position()
			elif not mb.pressed:
				_dragging = false
	elif event is InputEventMouseMotion and _dragging:
		var delta := DisplayServer.mouse_get_position() - _drag_start
		if delta.length() > 4.0:  # threshold: clique não vira arrasto
			_drag_start = DisplayServer.mouse_get_position()
			get_window().position += delta

func _pet_hovered() -> bool:
	var pet := get_tree().get_first_node_in_group("pet")
	return pet != null and pet.hovered

# ── Menu de contexto ─────────────────────────────────────────────────

func _show_context_menu(pos: Vector2) -> void:
	var menu := PopupMenu.new()
	var esc := PopupMenu.new()
	esc.name = "escala"
	esc.add_item("0,5×", 5)
	esc.add_item("1×", 10)
	esc.add_item("2×", 20)
	esc.id_pressed.connect(_on_scale_selected)
	menu.add_child(esc)
	menu.add_submenu_item("Escala", "escala")
	var z := PopupMenu.new()
	z.name = "zoom"
	z.add_item("1× (padrão)", 10)
	z.add_item("2×", 20)
	z.add_item("3×", 30)
	z.add_item("4×", 40)
	z.id_pressed.connect(_on_zoom_selected)
	menu.add_child(z)
	menu.add_submenu_item("Zoom (pixels maiores)", "zoom")
	menu.add_check_item("Sempre no topo", 1)
	menu.set_item_checked(1, get_window().always_on_top)
	menu.add_item("Ver status", 2)
	menu.add_separator()
	menu.add_item("Sair", 3)
	menu.id_pressed.connect(_on_menu_item.bind(menu))
	add_child(menu)
	menu.popup_hide.connect(menu.queue_free)
	menu.popup(Rect2i(Vector2i(pos), Vector2i.ZERO))

func _on_scale_selected(id: int) -> void:
	set_window_scale(id / 10.0)

# Zoom de câmera no World: pixels maiores, janela e viewport do jogo intactos.
func _on_zoom_selected(id: int) -> void:
	var world := get_tree().get_first_node_in_group("world")
	if world:
		world.zoom_factor = id / 10.0

func _zoom_step(d: float) -> void:
	var world := get_tree().get_first_node_in_group("world")
	if world:
		world.zoom_factor = clampf(world.zoom_factor + d, 1.0, 4.0)

func _on_menu_item(id: int, menu: PopupMenu) -> void:
	menu.queue_free()
	match id:
		1:
			var win := get_window()
			win.always_on_top = not win.always_on_top
		2:
			_show_status()
		3:
			quit_pet()

# ── Ações da janela ──────────────────────────────────────────────────

# Escala 0,5×/1×/2×: muda o tamanho da janela; o stretch canvas_items do
# projeto amplia o mesmo recorte (o bicho fica maior, não aparece mais mapa).
func set_window_scale(k: float) -> void:
	get_window().size = Vector2i(BASE_SIZE * k)

# F2 / bandeja: esconde ou mostra a janela (o bicho continua vivo escondido).
func _toggle_window() -> void:
	_hidden = not _hidden
	get_window().visible = not _hidden
	if _hidden:
		PetStats.save_data()  # salvamento por segurança ao esconder

# "Ver status" (menu/bandeja): mostra a janela e aciona o balão por 5 s.
func _show_status() -> void:
	if _hidden:
		_toggle_window()
	var pet := get_tree().get_first_node_in_group("pet")
	if pet:
		var balloon := pet.get_node_or_null("Balloon")
		if balloon:
			balloon.show_status(5.0)

func quit_pet() -> void:
	PetStats.save_data()
	get_tree().quit()

# ── Bandeja (Tarefa 10; noop quando a extensão não está instalada) ────

func _setup_tray() -> void:
	if not ClassDB.class_exists("TrayIcon"):
		return  # sem GDExtension (CI/WSL): roda sem bandeja
	_tray = ClassDB.instantiate("TrayIcon")
	_tray.show_icon(ProjectSettings.globalize_path("res://assets/tamapoke.ico"), "TamaPoke")
	_tray.connect("toggle_pressed", _toggle_window)
	_tray.connect("status_pressed", _show_status)
	_tray.connect("quit_pressed", quit_pet)
