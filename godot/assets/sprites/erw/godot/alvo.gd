extends AnimatedSprite2D
## Alvo de palha com estados de fogo. Chame atingir_com_fogo() quando o ataque de chama do Pokemon acertar.
## Animacoes: normal, em_chamas, fumegando, chamuscado, queimado.

signal estado_mudou(estado: StringName)

@export var duracao_chamas := 1.2
@export var duracao_fumaca := 2.0
@export var acertos_para_queimar := 5       # depois disso fica "queimado" ate restaurar()

var acertos := 0
var _token := 0

func _ready() -> void:
	_mudar(&"normal")

func atingir_com_fogo() -> void:
	acertos += 1
	_token += 1
	var meu := _token
	_mudar(&"em_chamas")
	await get_tree().create_timer(duracao_chamas).timeout
	if meu != _token: return
	_mudar(&"fumegando")
	await get_tree().create_timer(duracao_fumaca).timeout
	if meu != _token: return
	_mudar(&"queimado" if acertos >= acertos_para_queimar else &"chamuscado")

func restaurar() -> void:
	acertos = 0
	_token += 1
	_mudar(&"normal")

func _mudar(estado: StringName) -> void:
	play(estado)
	estado_mudou.emit(estado)
