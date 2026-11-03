extends PanelContainer
# Balão do HUD: aparece com o cursor sobre o bicho (ou por alguns segundos
# quando o jogador pede "Ver status"). Filho do Pokemon (espaço do mundo).

var _hunger: ProgressBar
var _energy: ProgressBar
var _xp: ProgressBar
var _level: Label
var _until_msec := -1

func _ready() -> void:
	visible = false
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	custom_minimum_size = Vector2(120, 66)
	position = Vector2(-60, -100)

	var vbox := VBoxContainer.new()
	vbox.add_theme_constant_override("separation", 3)
	add_child(vbox)

	_level = Label.new()
	_level.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_level.add_theme_font_size_override("font_size", 11)
	vbox.add_child(_level)

	_hunger = _make_bar(Color(0.95, 0.55, 0.15))
	_energy = _make_bar(Color(0.25, 0.65, 1.00))
	_xp     = _make_bar(Color(0.45, 0.85, 0.35))
	vbox.add_child(_hunger)
	vbox.add_child(_energy)
	vbox.add_child(_xp)

func _make_bar(fill_color: Color) -> ProgressBar:
	var bar := ProgressBar.new()
	bar.min_value = 0.0
	bar.max_value = 100.0
	bar.show_percentage = false
	bar.custom_minimum_size = Vector2(0, 10)
	var fill := StyleBoxFlat.new()
	fill.bg_color = fill_color
	bar.add_theme_stylebox_override("fill", fill)
	var bg := StyleBoxFlat.new()
	bg.bg_color = Color(0.12, 0.12, 0.12)
	bar.add_theme_stylebox_override("background", bg)
	return bar

# Mostra o balão por N segundos, mesmo sem hover (usado por "Ver status").
func show_status(seconds: float) -> void:
	_until_msec = Time.get_ticks_msec() + int(seconds * 1000.0)

func _process(_delta: float) -> void:
	var pet := get_parent()
	visible = pet.hovered or Time.get_ticks_msec() < _until_msec
	_hunger.value = PetStats.hunger
	_energy.value = PetStats.energy
	_xp.value     = PetStats.xp
	_level.text   = "Nv.%d" % PetStats.level
