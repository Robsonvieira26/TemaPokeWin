extends AnimatedSprite2D
## Borboleta independente: nasce em qualquer lugar da area e voa sozinha, com ritmo proprio,
## parando de vez em quando (asas fechadas) antes de seguir para outro ponto.

@export var area := Rect2(0, 0, 1088, 896)          # onde ela pode voar (coordenadas do mapa)
@export var comecar_em_lugar_aleatorio := true
@export var velocidade_min := 22.0                 # px/s
@export var velocidade_max := 55.0
@export var chance_de_pousar := 0.25              # ao chegar num ponto
@export var tempo_pousada := Vector2(1.0, 4.0)     # segundos (min, max)

var _rng := RandomNumberGenerator.new()
var _alvo: Vector2
var _vel := Vector2.ZERO
var _velocidade := 35.0
var _pousada := 0.0
var _fase := 0.0

func _ready() -> void:
	_rng.randomize()
	if comecar_em_lugar_aleatorio:
		position = Vector2(_rng.randf_range(area.position.x, area.end.x), _rng.randf_range(area.position.y, area.end.y))
	_fase = _rng.randf() * TAU
	speed_scale = _rng.randf_range(0.8, 1.3)
	_novo_alvo()
	play()

func _novo_alvo() -> void:
	var raio := _rng.randf_range(60.0, 260.0)
	var ang := _rng.randf() * TAU
	_alvo = (position + Vector2(cos(ang), sin(ang)) * raio).clamp(area.position, area.end)
	_velocidade = _rng.randf_range(velocidade_min, velocidade_max)

func _process(delta: float) -> void:
	if _pousada > 0.0:
		_pousada -= delta
		if _pousada <= 0.0:
			play(); _novo_alvo()
		return
	var desejada := (_alvo - position).normalized() * _velocidade
	_vel = _vel.lerp(desejada, clampf(delta * 1.8, 0.0, 1.0))
	_fase += delta * 7.0
	position += (_vel + Vector2(0.0, sin(_fase) * 14.0)) * delta   # sobe e desce batendo as asas
	if absf(_vel.x) > 2.0: flip_h = _vel.x < 0.0
	if position.distance_to(_alvo) < 6.0:
		if _rng.randf() < chance_de_pousar:
			_pousada = _rng.randf_range(tempo_pousada.x, tempo_pousada.y)
			pause(); frame = 1
		else:
			_novo_alvo()
