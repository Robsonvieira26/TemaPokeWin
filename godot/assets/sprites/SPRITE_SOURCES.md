# Fontes de Sprites

## Atual: PMD Collab (SkyTemple)

**URL:** https://sprites.pmdcollab.org  
**Repositório:** https://github.com/PMDCollab/SpriteCollab  
**Licença:** Gratuito com crédito aos artistas. Não usar em projetos comerciais.

Sprites no formato SkyTemple: sheets 8-direcionais com animações completas
(Idle, Walk, Sleep, Eat, Attack, Faint...). Ideal para desktop pet.

### Arquivos baixados (MVP)

| Pokémon     | #    | Idle | Walk | Sleep | Eat  |
|-------------|------|------|------|-------|------|
| Charmander  | 0004 | ✓    | ✓    | ✓     | ✓    |
| Charmeleon  | 0005 | ✓    | ✓    | ✓     | ✓    |
| Charizard   | 0006 | ✓    | ✓    | ✓     | ✗    |

Charizard não tem `Eat-Anim.png` no PMD Collab — controller usa `Idle` como fallback.

### Créditos por espécie
- `0004/credits.txt` — Charmander
- `0005/credits.txt` — Charmeleon
- `0006/credits.txt` — Charizard

---

## Futuro: PokeSprite (jogos oficiais)

**URL:** https://pokesprite.veekun.com  
**Repositório:** https://github.com/msikma/pokesprite  
**Licença:** Fan-use; sprites pertencem à Nintendo/Game Freak.

Sprites animados dos jogos oficiais (Sword/Shield, etc.) em GIF/APNG,
resolução maior (~96×96px), visual mais polido.

### Como migrar

1. Baixar GIFs do repositório `msikma/pokesprite` (pasta `animated/`).
2. Escrever script conversor (Python/GDScript) que:
   - Divide cada GIF em frames individuais (PNG)
   - Lê a duração de cada frame
   - Gera um `SpriteFrames` `.tres` por Pokémon
3. Em `pmd_sprite_loader.gd`, substituir `load_pokemon_frames()` por
   chamada ao novo loader de PokeSprite.
4. Remapear nomes de animação:
   - PokeSprite `"normal"` → controller `"Idle"`/`"Walk"` (ciclo único)
   - Sem equivalentes para `"Eat"`, `"Sleep"` → manter PMD Collab para essas

### Comparação

| Critério         | PMD Collab        | PokeSprite            |
|------------------|-------------------|-----------------------|
| Resolução        | ~32×40px          | ~96×96px              |
| Animações únicas | 30+ por Pokémon   | 1–2 por Pokémon       |
| Pokémon cobertos | ~900              | ~1000                 |
| Formato          | Sheet PNG + XML   | GIF/APNG por animação |
| Estilo           | PMD isométrico    | Batalha oficial       |
