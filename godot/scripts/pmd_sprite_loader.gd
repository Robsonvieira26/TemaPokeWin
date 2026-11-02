class_name PmdSpriteLoader
# Carrega spritesheets PMD Collab (formato SkyTemple) em SpriteFrames do Godot.
#
# Layout da sheet: 8 linhas (direções) × N colunas (frames da animação).
# Direções: South=0, SE=1, East=2, NE=3, North=4, NW=5, West=6, SW=7
#
# Para um pet 2D lateral usamos:
#   East (linha 2) → Walk, virado para a direita; flip horizontal para esquerda.
#   South (linha 0) → Idle, Eat, Sleep; de frente para o usuário.
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

const DIR_SOUTH := 0
const DIR_EAST  := 2

# Dados MVP das animações por espécie.
# "w"/"h": tamanho de um frame (px). "d": durações em ticks 30fps. "dir": linha de direção.
# null = animação não disponível → controller deve fazer fallback.
const ANIM_DATA: Dictionary = {
	"0004": {  # Charmander
		"Idle":  { "w": 32, "h": 40, "d": [12, 8, 8, 8],    "dir": DIR_SOUTH },
		"Walk":  { "w": 32, "h": 32, "d": [6,  8, 6, 8],    "dir": DIR_EAST  },
		"Sleep": { "w": 32, "h": 24, "d": [30, 35],          "dir": DIR_SOUTH },
		"Eat":   { "w": 24, "h": 32, "d": [6,  8, 6, 8],    "dir": DIR_SOUTH },
	},
	"0005": {  # Charmeleon
		"Idle":  { "w": 32, "h": 56, "d": [40, 2, 3, 3, 3, 2], "dir": DIR_SOUTH },
		"Walk":  { "w": 24, "h": 32, "d": [8,  10, 8, 10],     "dir": DIR_EAST  },
		"Sleep": { "w": 24, "h": 32, "d": [30, 35],              "dir": DIR_SOUTH },
		"Eat":   { "w": 24, "h": 32, "d": [6,  8,  6,  8],     "dir": DIR_SOUTH },
	},
	"0006": {  # Charizard
		"Idle":  { "w": 40, "h": 48, "d": [15, 15, 15, 15], "dir": DIR_SOUTH },
		"Walk":  { "w": 40, "h": 48, "d": [8,  10, 8,  10], "dir": DIR_EAST  },
		"Sleep": { "w": 32, "h": 48, "d": [30, 35],          "dir": DIR_SOUTH },
		"Eat":   null,  # indisponível no PMD Collab — controller usa Idle como fallback
	},
}

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
		var tex := load(path) as Texture2D
		if tex == null:
			push_warning("PmdSpriteLoader: arquivo não encontrado: %s" % path)
			continue
		_add_anim(frames, anim_name, tex, spec)

	return frames

static func _add_anim(
	frames: SpriteFrames,
	anim_name: String,
	texture: Texture2D,
	spec: Dictionary
) -> void:
	frames.add_animation(anim_name)
	frames.set_animation_loop(anim_name, true)
	frames.set_animation_speed(anim_name, TICK_RATE)

	var fw: int     = spec["w"]
	var fh: int     = spec["h"]
	var durs: Array = spec["d"]
	var row: int    = spec["dir"]
	var y := row * fh

	for i: int in durs.size():
		var atlas := AtlasTexture.new()
		atlas.atlas  = texture
		atlas.region = Rect2(i * fw, y, fw, fh)
		frames.add_frame(anim_name, atlas, float(durs[i]))
