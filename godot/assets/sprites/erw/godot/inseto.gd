extends AnimatedSprite2D
## Voo de borboleta/abelha em volta do ponto onde foi colocado.

@export var raio := Vector2(16, 8)
@export var velocidade := 0.6   # voltas por segundo (abelhas usam mais)
var _origem: Vector2
var _fase: float

func _ready() -> void:
	_origem = position
	_fase = fmod(position.x * 0.37 + position.y * 0.11, TAU)
	play()

func _process(_delta: float) -> void:
	var t := Time.get_ticks_msec() / 1000.0 * velocidade * TAU + _fase
	position = _origem + Vector2(raio.x * sin(t) + raio.x * 0.25 * sin(3.0 * t),
		-raio.y - raio.y * 0.8 * absf(sin(2.0 * t + _fase)) + 3.0 * cos(t))
