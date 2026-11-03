class_name DayNight
extends CanvasModulate
# Tint da hora local do sistema sobre o jardim (spec, seção 5).
# 7h–8h manhã · 8h–18h pleno · 18h–20h entardecer · 20h–22h anoitecer · 22h–7h noite.

const COL_DAY   := Color(1.0, 1.0, 1.0)
const COL_DUSK  := Color(0.9, 0.72, 0.55)   # entardecer quente
const COL_NIGHT := Color(0.35, 0.42, 0.58)  # azul escuro (~35% de brilho)

# Pura e testável: cor do tint para a hora local dada.
static func tint_for_hour(hour: float) -> Color:
	if hour >= 8.0 and hour < 18.0:
		return COL_DAY
	if hour >= 18.0 and hour < 20.0:
		return COL_DAY.lerp(COL_DUSK, (hour - 18.0) / 2.0)
	if hour >= 20.0 and hour < 22.0:
		return COL_DUSK.lerp(COL_NIGHT, (hour - 20.0) / 2.0)
	if hour >= 22.0 or hour < 7.0:
		return COL_NIGHT
	return COL_NIGHT.lerp(COL_DAY, hour - 7.0)  # 7h–8h: amanhecer

func _process(_delta: float) -> void:
	# Valores de Dictionary são Variant: converte para float explicitamente.
	var t := Time.get_time_dict_from_system()
	var hour: float = float(t.hour) + float(t.minute) / 60.0
	modulate = tint_for_hour(hour)
