class_name PmdSpriteLoader
# Carrega spritesheets PMD Collab (formato SkyTemple) em SpriteFrames do Godot.
#
# Layout da sheet: 8 linhas (direções) × N colunas (frames da animação).
# Direções: South=0, SE=1, East=2, NE=3, North=4, NW=5, West=6, SW=7
#
# Cada sheet é um grid: linha = direção (0=South, sentido horário até 7=SW).
# ANIM_DATA abaixo mapeia espécie → specs de animação ("dir" linha única ou
# "dirs" todas as 8, criando "<Nome><linha>").
#
# ─────────────────────────────────────────────────────────────────────
# FUTURO – alternativa PokeSprite (pokesprite.veekun.com / github.com/msikma/pokesprite):
#   Sprites animados dos jogos oficiais em GIF/APNG, resolução maior (96×96+).
#   Um script conversor dividiria cada GIF em frames e montaria SpriteFrames.
#   Vantagem: arte oficial, mais Pokémon, resolução mais alta.
#   Desvantagem: poucas animações únicas (geralmente só frente de batalha;
#                sem variantes Eat/Sleep próprias).
#   Para trocar: substitua a chamada load_pokemon_frames() por um loader
#   do PokeSprite e remapeie os nomes de animação conforme o controller espera.
# ─────────────────────────────────────────────────────────────────────

const TICK_RATE := 30.0

# Layout SkyTemple: linha 0 = South (de frente para baixo), cresce no sentido
# horário: 1=SE, 2=East, 3=NE, 4=North, 5=NW, 6=West, 7=SW.
const DIR_SOUTH := 0
const ALL_DIRS := [0, 1, 2, 3, 4, 5, 6, 7]

# Dados MVP das animações por espécie.
# "w"/"h": frame (px). "d": durações em ticks 30fps.
# "dir": linha única (animações de frente). "dirs": cria "<Nome><linha>" por direção.
# null = animação indisponível → controller faz fallback.
const ANIM_DATA: Dictionary = {
	"0004": {  # Charmander
		"Idle":  { "w": 32, "h": 40, "d": [12, 8, 8, 8], "dir": DIR_SOUTH },
		"Walk":  { "w": 32, "h": 32, "d": [6, 8, 6, 8],  "dirs": ALL_DIRS },
		"Sleep": { "w": 32, "h": 24, "d": [30, 35],      "dir": DIR_SOUTH },
		"Eat":   { "w": 24, "h": 32, "d": [6, 8, 6, 8],  "dir": DIR_SOUTH },
	},
	"0005": {  # Charmeleon
		"Idle":  { "w": 32, "h": 56, "d": [40, 2, 3, 3, 3, 2], "dir": DIR_SOUTH },
		"Walk":  { "w": 24, "h": 32, "d": [8, 10, 8, 10],      "dirs": ALL_DIRS },
		"Sleep": { "w": 24, "h": 32, "d": [30, 35],            "dir": DIR_SOUTH },
		"Eat":   { "w": 24, "h": 32, "d": [6, 8, 6, 8],        "dir": DIR_SOUTH },
	},
	"0006": {  # Charizard
		"Idle":  { "w": 40, "h": 48, "d": [15, 15, 15, 15], "dir": DIR_SOUTH },
		"Walk":  { "w": 40, "h": 48, "d": [8, 10, 8, 10],   "dirs": ALL_DIRS },
		"Sleep": { "w": 32, "h": 48, "d": [30, 35],         "dir": DIR_SOUTH },
		"Eat":   null,  # indisponível no PMD Collab — controller usa Idle como fallback
	},
}

# Velocidade → direção SkyTemple. angle() em GDScript: 0 = Leste, +90 = Sul
# (y cresce para baixo). A linha cresce no sentido horário a partir do Sul.
static func direction_for_velocity(v: Vector2) -> int:
	if v.length_squared() < 0.0001:
		return DIR_SOUTH
	var a := rad_to_deg(v.angle())
	return wrapi(roundi((90.0 - a) / 45.0), 0, 8)

static func load_pokemon_frames(species_id: String) -> SpriteFrames:
	var data: Dictionary = ANIM_DATA.get(species_id, {})
	if data.is_empty():
		push_error("PmdSpriteLoader: espécie desconhecida: %s" % species_id)
		return null

	var base := "res://assets/sprites/%s" % species_id
	var frames := SpriteFrames.new()
	frames.remove_animation("default")

	for anim_name: String in data:
		var spec = data[anim_name]
		if spec == null:
			continue
		var path := "%s/%s-Anim.png" % [base, anim_name]
		var tex := _load_tex(path)
		if tex == null:
			push_warning("PmdSpriteLoader: arquivo não encontrado: %s" % path)
			continue
		if spec.has("dirs"):
			for row: int in spec["dirs"]:
				_add_anim(frames, "%s%d" % [anim_name, row], tex, spec, row)
		else:
			_add_anim(frames, anim_name, tex, spec, spec["dir"])

	return frames

static func _load_tex(res_path: String) -> Texture2D:
	# Usa Image.load_from_file para funcionar sem cache de import do editor.
	var abs := ProjectSettings.globalize_path(res_path)
	var img := Image.load_from_file(abs)
	if img == null:
		return null
	return ImageTexture.create_from_image(img)

static func _add_anim(
	frames: SpriteFrames,
	anim_name: String,
	texture: Texture2D,
	spec: Dictionary,
	row: int
) -> void:
	frames.add_animation(anim_name)
	frames.set_animation_loop(anim_name, true)
	frames.set_animation_speed(anim_name, TICK_RATE)

	var fw: int     = spec["w"]
	var fh: int     = spec["h"]
	var durs: Array = spec["d"]
	var y := row * fh

	for i: int in durs.size():
		var atlas := AtlasTexture.new()
		atlas.atlas  = texture
		atlas.region = Rect2(i * fw, y, fw, fh)
		frames.add_frame(anim_name, atlas, float(durs[i]))
