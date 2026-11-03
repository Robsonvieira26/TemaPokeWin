shader_type canvas_item;
// Vento em pixel art: cada planta tem fase, ritmo e sensibilidade proprios (pela posicao no mundo).
// Quase parado em repouso; rajadas passam por faixas do mapa e somem.
uniform float amplitude_px = 1.8;   // deslocamento maximo no topo, em pixels
uniform float rigidez = 1.3;        // >1 = so a copa mexe, base firme
uniform float frequencia = 0.6;     // oscilacoes por segundo
uniform float atraso_topo = 0.25;   // segundos que o topo atrasa em relacao a base
uniform float brisa = 0.22;         // movimento de repouso (0 = parado)
uniform vec2 direcao_rajada = vec2(1.0, 0.25);
uniform float velocidade_rajada = 70.0;   // px/s
uniform float intervalo_rajada = 3.2;     // s entre rajadas
uniform float largura_rajada = 160.0;     // px: largura da faixa atingida
uniform float margem_px = 3.0;

varying vec2 world_pos;

float h(vec2 p, float s) { return fract(sin(dot(p, vec2(12.9898, 78.233)) + s * 37.719) * 43758.5453); }

void vertex() {
    world_pos = (MODEL_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xy;
    float w = 1.0 / TEXTURE_PIXEL_SIZE.x;
    float pad_uv = margem_px / w;
    VERTEX.x += (UV.x * 2.0 - 1.0) * margem_px;           // alarga o quad para nada ser cortado
    UV.x = UV.x * (1.0 + 2.0 * pad_uv) - pad_uv;
}

float rajada(float t, vec2 p) {
    vec2 d = normalize(direcao_rajada);
    float along = dot(p, d);
    float across = dot(p, vec2(-d.y, d.x));
    float phase = (t * velocidade_rajada - along) / (intervalo_rajada * velocidade_rajada);
    float k = floor(phase); float f = fract(phase);
    float env = exp(-pow((f - 0.5) * 6.0, 2.0));
    float centro = (h(vec2(k, 1.0), 2.0) - 0.5) * 900.0;  // cada rajada pega uma faixa diferente
    float lat = exp(-pow((across - centro) / largura_rajada, 2.0));
    return env * lat * (0.6 + 0.6 * h(vec2(k, 3.0), 4.0));
}

void fragment() {
    vec2 uv = UV;
    float k = clamp(1.0 - uv.y, 0.0, 1.0);                  // 0 na base, 1 no topo
    float ph = h(world_pos, 1.0) * 6.2831;
    float fr = frequencia * (0.75 + 0.5 * h(world_pos, 2.0));
    float sens = 0.35 + 0.85 * h(world_pos, 3.0);
    float lado = h(world_pos, 4.0) > 0.2 ? 1.0 : -0.6;
    float t = TIME - atraso_topo * k;
    float s = brisa * sin(6.2831 * fr * t + ph)
            + rajada(t, world_pos) * sens * (0.45 * lado + 0.55 * sin(6.2831 * fr * 1.7 * t + ph));
    float dx = round(amplitude_px * pow(k, rigidez) * s);   // desloca em pixels inteiros
    uv.x -= dx * TEXTURE_PIXEL_SIZE.x;
    if (uv.x < 0.0 || uv.x > 1.0) discard;
    COLOR = texture(TEXTURE, uv);
}
