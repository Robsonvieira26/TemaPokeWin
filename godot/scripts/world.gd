extends Node2D
# Dono do lugar: constrói o polígono navegável a partir de retângulos autorados
# (grama livre + pontes) e expõe os pontos de vagância do bicho.
# O World entra no grupo "world" para o controller achá-lo.

@export var walkable_rects: Array[Rect2] = []
@export var wander_points: PackedVector2Array = []

func _ready() -> void:
	var poly := NavigationPolygon.new()
	for r in walkable_rects:
		poly.add_outline(PackedVector2Array([
			r.position,
			Vector2(r.end.x, r.position.y),
			r.end,
			Vector2(r.position.x, r.end.y),
		]))
	# Deprecated em 4.2+, mas ainda é o caminho de uma linha para outlines de retângulo;
	# o warning no output é esperado e inofensivo.
	poly.make_polygons_from_outlines()
	$NavRegion.navigation_polygon = poly

func random_wander_point() -> Vector2:
	return wander_points.pick_random()
