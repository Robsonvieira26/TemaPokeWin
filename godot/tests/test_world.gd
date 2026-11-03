extends "res://tests/test_base.gd"
# Testa o World com o cenário Vale Termal importado do PokePet:
# habitat instanciado, zonas nas marcas do vale e navegação derivada
# do mapa de colisão embutido na cena (Colisao: Agua/Penhascos/Objetos).

const SPAWN := Vector2(950, 723)  # Spawn do vale_termal.tscn

func _fresh_world() -> Node:
	return load("res://scenes/World.tscn").instantiate()

func _centroide(p: PackedVector2Array) -> Vector2:
	var c := Vector2.ZERO
	for v in p:
		c += v
	return c / p.size()

func _poligono_nav(poly: NavigationPolygon, i: int) -> PackedVector2Array:
	# get_polygon(i) devolve ÍNDICES em get_vertices(); converte para pontos.
	var verts: PackedVector2Array = poly.get_vertices()
	var pts := PackedVector2Array()
	for j in poly.get_polygon(i):
		pts.append(verts[j])
	return pts

func test_world_instancia_habitat_e_zonas() -> void:
	var w := _fresh_world()
	assert_true(w.get_node_or_null("Habitat") != null, "habitat instanciado no World")
	var nomes := {}
	for n in w.get_children():
		if n.is_in_group("activity_zones"):
			nomes[n.get("activity_name")] = true
	assert_true(nomes.has("eat"), "zona de comer (comedouro do vale)")
	assert_true(nomes.has("train"), "zona de treino (clareira de treino)")
	assert_true(nomes.has("sleep"), "zona de dormir (cama/abrigo)")
	w.free()

func test_nav_derivado_do_mapa_de_colisao() -> void:
	var w := _fresh_world()
	assert_true(w._obstacle_outlines().size() >= 100, "colisões do habitat coletadas (111 esperados)")
	var poly: NavigationPolygon = w._build_nav_polygon()
	assert_true(poly.get_polygon_count() > 0, "polígonos navegáveis gerados")
	var pontos := [SPAWN, Vector2(830, 675), Vector2(915, 832), Vector2(1020, 648)]
	var navegaveis := {}
	for i in poly.get_polygon_count():
		for p in pontos:
			if Geometry2D.is_point_in_polygon(p, _poligono_nav(poly, i)):
				navegaveis[p] = true
	for p in pontos:
		assert_true(navegaveis.has(p), "ponto %s navegável (spawn e centros de zona)" % p)
	# centroide de um polígono de água não pode cair na área navegável
	var agua: CollisionPolygon2D = w.get_node("Habitat/Colisao/Agua/agua_1")
	var c := _centroide(agua.polygon)
	var agua_outside := true
	for i in poly.get_polygon_count():
		if Geometry2D.is_point_in_polygon(c, _poligono_nav(poly, i)):
			agua_outside = false
	assert_true(agua_outside, "água fora da área navegável")
	w.free()

func test_spawn_e_zonas_fora_de_colisao() -> void:
	var w := _fresh_world()
	var outlines: Array[PackedVector2Array] = w._obstacle_outlines()
	assert_true(outlines.size() > 0, "obstáculos coletados do habitat")
	var pontos := {
		"spawn": SPAWN,
		"comedouro": Vector2(830, 675),
		"treino": Vector2(915, 832),
		"cama": Vector2(1020, 648),
	}
	for nome in pontos:
		for pts in outlines:
			assert_true(not Geometry2D.is_point_in_polygon(pontos[nome], pts),
					"%s (%s) não pode estar dentro de colisão" % [nome, pontos[nome]])
	w.free()