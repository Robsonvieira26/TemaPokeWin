extends CanvasLayer

# HUD de stats do pet: fome, energia, nível/XP.
# Montado em código para evitar dependência de theme externo.

var _hunger_bar: ProgressBar
var _energy_bar: ProgressBar
var _xp_bar:     ProgressBar
var _level_lbl:  Label

func _ready() -> void:
	layer = 10  # acima de tudo

	var panel := PanelContainer.new()
	panel.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	panel.custom_minimum_size = Vector2(480, 44)
	panel.modulate = Color(1.0, 1.0, 1.0, 0.80)
	add_child(panel)

	var hbox := HBoxContainer.new()
	hbox.add_theme_constant_override("separation", 6)
	panel.add_child(hbox)

	# label de nível
	_level_lbl = Label.new()
	_level_lbl.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	_level_lbl.custom_minimum_size = Vector2(52, 0)
	_level_lbl.add_theme_font_size_override("font_size", 11)
	hbox.add_child(_level_lbl)

	# barras
	_hunger_bar = _make_bar(Color(0.95, 0.55, 0.15))
	_energy_bar = _make_bar(Color(0.25, 0.65, 1.00))
	_xp_bar     = _make_bar(Color(0.45, 0.85, 0.35))

	hbox.add_child(_make_col("Fome",    _hunger_bar))
	hbox.add_child(_make_col("Energia", _energy_bar))
	hbox.add_child(_make_col("XP",      _xp_bar))

func _make_bar(fill_color: Color) -> ProgressBar:
	var bar := ProgressBar.new()
	bar.min_value = 0.0
	bar.max_value = 100.0
	bar.show_percentage = false
	bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bar.custom_minimum_size   = Vector2(0, 12)
	# colorir via StyleBoxFlat
	var fill := StyleBoxFlat.new()
	fill.bg_color = fill_color
	fill.corner_radius_top_left     = 3
	fill.corner_radius_top_right    = 3
	fill.corner_radius_bottom_left  = 3
	fill.corner_radius_bottom_right = 3
	bar.add_theme_stylebox_override("fill", fill)
	return bar

func _make_col(label_text: String, bar: ProgressBar) -> VBoxContainer:
	var col := VBoxContainer.new()
	col.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	col.add_theme_constant_override("separation", 2)
	var lbl := Label.new()
	lbl.text = label_text
	lbl.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	lbl.add_theme_font_size_override("font_size", 10)
	col.add_child(lbl)
	col.add_child(bar)
	return col

func _process(_delta: float) -> void:
	_hunger_bar.value = PetStats.hunger
	_energy_bar.value = PetStats.energy
	_xp_bar.value     = PetStats.xp
	_level_lbl.text   = "Nv.%d" % PetStats.level
