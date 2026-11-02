extends Area2D

# Aplicado a cada Area2D de atividade no mapa.
# O pokémon decide entrar/sair; a zona só declara o que faz.

@export var activity_name: String = "eat"
@export var xp_per_second: float  = 2.0
@export var stat_restore:  String = ""   # "hunger" | "energy" | ""
@export var restore_rate:  float  = 5.0  # unidades/s restauradas

func _ready() -> void:
	body_entered.connect(_on_body_entered)
	body_exited.connect(_on_body_exited)

func _on_body_entered(body: Node2D) -> void:
	if body.has_method("enter_activity"):
		body.enter_activity(self)

func _on_body_exited(body: Node2D) -> void:
	if body.has_method("exit_activity"):
		body.exit_activity(self)
