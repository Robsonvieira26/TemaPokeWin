extends Node2D
# Dono do lugar: monta a navegação a partir do cenário Vale Termal
# (lar do Charmander, importado do PokePet). O chão navegável é o mapa
# inteiro menos os polígonos de colisão embutidos na cena do habitat
# (nó Habitat/Colisao: água, penhascos e objetos).
# O World entra no grupo "world" para o controller achá-lo.

const MAP_RECT := Rect2(8, 8, 1072, 880)   # mapa 1088x896 com margem de segurança
const FALLBACK_POINT := Vector2(950, 723)  # spawn do vale_termal

func _ready() -> void:
	$NavRegion.navigation_polygon = _build_nav_polygon()

func _build_nav_polygon() -> NavigationPolygon:
	var poly := NavigationPolygon.new()
	poly.add_outline(_rect_outline(MAP_RECT))
	for pts in _obstacle_outlines():
		poly.add_outline(pts)
	# Deprecated em 4.2+, mas é o caminho direto para outlines autorados;
	# o warning no output é esperado e inofensivo.
	poly.make_polygons_from_outlines()
	return poly

func _rect_outline(r: Rect2) -> PackedVector2Array:
	return PackedVector2Array([
		r.position,
		Vector2(r.end.x, r.position.y),
		r.end,
		Vector2(r.position.x, r.end.y),
	])

# Coleta os polígonos de colisão do habitat já transformados para o espaço
# do World (só com transforms locais acumulados: funciona fora da árvore).
func _obstacle_outlines() -> Array[PackedVector2Array]:
	var out: Array[PackedVector2Array] = []
	var colisao: Node = $Habitat.get_node_or_null("Colisao")
	if colisao == null:
		return out
	_collect(colisao, Transform2D.IDENTITY, out)
	return out

func _collect(node: Node, xform: Transform2D, out: Array[PackedVector2Array]) -> void:
	if node is Node2D:
		xform = xform * (node as Node2D).transform
	if node is CollisionPolygon2D:
		var pts := PackedVector2Array()
		for p in (node as CollisionPolygon2D).polygon:
			pts.append(xform * p)
		if pts.size() >= 3:
			out.append(pts)
		return  # CollisionPolygon2D não tem filhos relevantes
	for child in node.get_children():
		_collect(child, xform, out)

func random_wander_point() -> Vector2:
	# Amostra o mapa e ajusta ao ponto navegável mais próximo; se a amostra
	# cair em água/objeto (longe demais de área livre), tenta de novo.
	var map := get_world_2d().navigation_map
	for i in 10:
		var p := Vector2(randf_range(24, 1064), randf_range(24, 872))
		var q := NavigationServer2D.map_get_closest_point(map, p)
		if q.distance_to(p) < 48.0:
			return q
	return FALLBACK_POINT

func _process(_delta: float) -> void:
	$Camera.position = $Pokemon.position