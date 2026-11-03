extends Node
# Autoload: PetStats — dono do estado do bicho (fome, energia, XP/nível),
# do relógio do mundo e do save. Fonte única de verdade: os outros nós só
# leem daqui, escutam os sinais ou pedem ações (try_pet, restore_*).

signal stats_changed
signal level_changed(new_level: int)

const SAVE_PATH := "user://pet_save.cfg"

# ── Tuning (spec, seção 5) ────────────────────────────────────────────
const XP_PER_LEVEL         := 100.0
const HUNGER_DECAY_AWAKE   := 0.00463  # 100 → 0 em ~6 h acordado
const HUNGER_DECAY_ASLEEP  := 0.002315 # dormindo, metade do ritmo
const ENERGY_DECAY         := 0.00174  # 100 → 0 em ~16 h acordado
const ENERGY_RESTORE_SLEEP := 0.0139   # 0 → 100 em ~2 h de cama
const HUNGER_LOW           := 30.0
const ENERGY_LOW           := 20.0
const PET_XP               := 2.0      # carinho
const PET_COOLDOWN         := 30.0     # s entre carinhos que contam
const NIGHT_START          := 22.0     # hora local
const NIGHT_END            := 7.0
const OFFLINE_STEP         := 300.0    # s por passo da simulação offline
const OFFLINE_MIN          := 300.0    # absências menores que isso são ignoradas
const OFFLINE_XP_CAP       := 300.0    # teto de XP por absência (~3 níveis)
const OFFLINE_XP_PER_STEP  := 1.0

var xp: float     = 0.0
var level: int    = 1
var hunger: float = 100.0
var energy: float = 100.0
var asleep := false            # setado pelo controller (bicho na cama)
var utc_offset_hours := 0.0    # fuso local; _ready calcula, testes sobrescrevem
var save_path: String = SAVE_PATH
var hour_override := -1.0  # >= 0 força a hora local (debug via aba Remote do editor)

var _pet_ready_at := -INF  # engine-time (s) em que o próximo carinho conta
var _auto_save: Timer

func _ready() -> void:
	utc_offset_hours = _system_utc_offset()
	var saved_at := load_data()
	if saved_at > 0.0:
		simulate_offline(saved_at, now_epoch())
	_auto_save = Timer.new()
	_auto_save.wait_time = 60.0
	_auto_save.autostart = true
	_auto_save.timeout.connect(save_data)
	add_child(_auto_save)

func _process(delta: float) -> void:
	tick(delta)

# Decaimento em tempo real. O controller liga `asleep` quando o bicho está na cama.
func tick(delta: float) -> void:
	if asleep:
		hunger = maxf(0.0,  hunger - HUNGER_DECAY_ASLEEP * delta)
		energy = minf(100.0, energy + ENERGY_RESTORE_SLEEP * delta)
	else:
		hunger = maxf(0.0,  hunger - HUNGER_DECAY_AWAKE * delta)
		energy = maxf(0.0,  energy - ENERGY_DECAY * delta)
	stats_changed.emit()

func add_xp(amount: float, source: String = "") -> void:
	xp += amount
	while xp >= XP_PER_LEVEL:
		xp    -= XP_PER_LEVEL
		level += 1
		level_changed.emit(level)
		print("[PetStats] Level up! Nível ", level, " (fonte: ", source, ")")

func restore_hunger(amount: float) -> void:
	hunger = minf(100.0, hunger + amount)

func restore_energy(amount: float) -> void:
	energy = minf(100.0, energy + amount)

# Carinho: devolve true se o carinho contou (fora do cooldown).
func try_pet(now_sec: float) -> bool:
	if now_sec < _pet_ready_at:
		return false
	_pet_ready_at = now_sec + PET_COOLDOWN
	add_xp(PET_XP, "carinho")
	return true

func is_night(hour: float) -> bool:
	return hour >= NIGHT_START or hour < NIGHT_END

func now_epoch() -> float:
	return Time.get_unix_time_from_system()

# ── Offline (spec, seção 5): a IA em avanço rápido ────────────────────
# Passos de OFFLINE_STEP s com as regras da IA: de noite dorme; de dia come
# se a fome está baixa, senão treina (XP com teto). Necessidades sempre se
# resolvem — o bicho nunca fica "ferido" nem morre.
func simulate_offline(start_epoch: float, now_epoch: float) -> void:
	var elapsed := now_epoch - start_epoch
	if elapsed < OFFLINE_MIN:
		return
	var xp_offline := 0.0
	var steps := int(elapsed / OFFLINE_STEP)
	for i in steps:
		var hour := _local_hour(start_epoch + (i + 1) * OFFLINE_STEP)
		if is_night(hour):
			energy = minf(100.0, energy + ENERGY_RESTORE_SLEEP * OFFLINE_STEP)
			hunger = maxf(0.0,  hunger - HUNGER_DECAY_ASLEEP * OFFLINE_STEP)
		elif hunger < HUNGER_LOW:
			hunger = 100.0  # comeu a refeição inteira no passo
		elif xp_offline < OFFLINE_XP_CAP:
			xp_offline += OFFLINE_XP_PER_STEP
			add_xp(OFFLINE_XP_PER_STEP, "treino offline")
	asleep = false
	if energy < ENERGY_LOW:
		energy = 100.0  # chegou da absência descansado
	stats_changed.emit()

func _local_hour(epoch: float) -> float:
	return fposmod(epoch / 3600.0 + utc_offset_hours, 24.0)

# Hora local do mundo agora — respeita o override de debug.
func local_hour_now() -> float:
	if hour_override >= 0.0:
		return hour_override
	return _local_hour(now_epoch())

# Fuso local do sistema, a partir da diferença relógio local × UTC.
func _system_utc_offset() -> float:
	# Valores de Dictionary são Variant: converte para float explicitamente.
	var local := Time.get_time_dict_from_system()
	var utc   := Time.get_time_dict_from_system(true)
	var diff: float = (float(local.hour) + float(local.minute) / 60.0) - (float(utc.hour) + float(utc.minute) / 60.0)
	return fposmod(diff + 12.0, 24.0) - 12.0

# ── Save ──────────────────────────────────────────────────────────────
func save_data() -> void:
	var cfg := ConfigFile.new()
	cfg.set_value("stats", "xp", xp)
	cfg.set_value("stats", "level", level)
	cfg.set_value("stats", "hunger", hunger)
	cfg.set_value("stats", "energy", energy)
	cfg.set_value("stats", "saved_at_epoch", now_epoch())
	cfg.save(save_path)

# Devolve a época (UTC) do último save; 0 se não havia save.
func load_data() -> float:
	var cfg := ConfigFile.new()
	if cfg.load(save_path) != OK:
		return 0.0
	xp     = cfg.get_value("stats", "xp", 0.0)
	level  = cfg.get_value("stats", "level", 1)
	hunger = maxf(HUNGER_LOW, cfg.get_value("stats", "hunger", 100.0))
	energy = maxf(ENERGY_LOW, cfg.get_value("stats", "energy", 100.0))
	return cfg.get_value("stats", "saved_at_epoch", 0.0)
