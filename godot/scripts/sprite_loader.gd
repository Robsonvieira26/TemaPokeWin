extends Node

# Carrega os spritesheets do PMDCollab e monta um SpriteFrames em runtime.
#
# Formato PMDCollab: cada PNG é um grid de (frames × direções).
# Usamos a direção "Right" (linha 2 de 8) para todos os sprites direcionais;
# o pokemon_controller inverte flip_h para andar para a esquerda.
#
# Fonte: https://sprites.pmdcollab.org/  (CC BY-NC 4.0)
# Opção futura: integrar pokesprite (https://github.com/msikma/pokesprite)
#   para gerar assets animados adicionais via ferramentas de conversão
#   PNG-estático → spritesheet animado.

const BASE_PATH   := "res://assets/sprites/charmander/"
const ANIM_FPS    := 8.0  # velocidade padrão; ajuste por animação se necessário

# Definições: { nome_godot: { file, fw, fh, frames, dir_row } }
# dir_row = linha da direção a usar; -1 = sprite não direcional (única linha)
const ANIM_DEFS := {
	"walk":   { "file": "Walk-Anim.png",   "fw": 32, "fh": 32, "frames": 4, "dir_row": 2 },
	"idle":   { "file": "Idle-Anim.png",   "fw": 32, "fh": 40, "frames": 4, "dir_row": 2 },
	"sleep":  { "file": "Sleep-Anim.png",  "fw": 32, "fh": 24, "frames": 2, "dir_row": -1 },
	"eat":    { "file": "Eat-Anim.png",    "fw": 24, "fh": 32, "frames": 4, "dir_row": -1 },
	"train":  { "file": "Charge-Anim.png", "fw": 32, "fh": 40, "frames": 10, "dir_row": 2 },
}

func build_sprite_frames() -> SpriteFrames:
	var sf := SpriteFrames.new()
	sf.remove_animation("default")

	for anim_name in ANIM_DEFS:
		var d: Dictionary = ANIM_DEFS[anim_name]
		var tex: Texture2D = load(BASE_PATH + d["file"])
		if tex == null:
			push_warning("SpriteLoader: não encontrou " + d["file"])
			continue

		sf.add_animation(anim_name)
		sf.set_animation_speed(anim_name, ANIM_FPS)
		sf.set_animation_loop(anim_name, true)

		var y_offset: int = 0
		if d["dir_row"] >= 0:
			y_offset = d["dir_row"] * d["fh"]

		for i in range(d["frames"]):
			var region := Rect2(i * d["fw"], y_offset, d["fw"], d["fh"])
			var atlas := AtlasTexture.new()
			atlas.atlas = tex
			atlas.region = region
			sf.add_frame(anim_name, atlas)

	return sf
