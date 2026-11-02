extends Node

# Autoload: PetStats — stats globais do pet, persistidos em user://

const SAVE_PATH    := "user://pet_save.cfg"
const XP_PER_LEVEL := 100.0

var xp:     float = 0.0
var level:  int   = 1
var hunger: float = 100.0  # 0–100
var energy: float = 100.0  # 0–100

func _ready() -> void:
	load_data()
	var t := Timer.new()
	t.wait_time = 60.0
	t.autostart = true
	t.timeout.connect(save_data)
	add_child(t)

func _process(delta: float) -> void:
	hunger = maxf(0.0, hunger - 0.5  * delta)
	energy = maxf(0.0, energy - 0.15 * delta)

func add_xp(amount: float, source: String = "") -> void:
	xp += amount
	while xp >= XP_PER_LEVEL:
		xp    -= XP_PER_LEVEL
		level += 1
		print("[PetStats] Level up! Nível ", level, " (fonte: ", source, ")")

func restore_hunger(amount: float) -> void:
	hunger = minf(100.0, hunger + amount)

func restore_energy(amount: float) -> void:
	energy = minf(100.0, energy + amount)

func save_data() -> void:
	var cfg := ConfigFile.new()
	cfg.set_value("stats", "xp",     xp)
	cfg.set_value("stats", "level",  level)
	cfg.set_value("stats", "hunger", hunger)
	cfg.set_value("stats", "energy", energy)
	cfg.save(SAVE_PATH)

func load_data() -> void:
	var cfg := ConfigFile.new()
	if cfg.load(SAVE_PATH) != OK:
		return
	xp     = cfg.get_value("stats", "xp",     0.0)
	level  = cfg.get_value("stats", "level",  1)
	hunger = maxf(10.0, cfg.get_value("stats", "hunger", 100.0))
	energy = maxf(10.0, cfg.get_value("stats", "energy", 100.0))
