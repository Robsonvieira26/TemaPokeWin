# Verificação pendente — rodar no Windows

Branch: `claude/godot-sim-mvp` (Tasks 0–10 do plano
`docs/superpowers/plans/2026-11-02-godot-sim-mvp.md` implementadas e revisadas).
Nada foi executado em WSL (Godot indisponível) — tudo abaixo é verificação real.

## 1. Suíte headless

```powershell
# em clone fresco, rode o --import UMA vez antes (gera o cache de classes
# globais de PmdSpriteLoader/DayNight — sem isso os testes não compilam)
godot --headless --path godot --import
godot --headless --path godot --script res://tests/run_all.gd
```

Esperado: `OK — todos os testes passaram`; exit 0.

## 2. Build da extensão da bandeja (GDExtension)

No PowerShell (precisa de MSVC + SCons instalados):

```powershell
cd godot\src\tray
git clone --branch master https://github.com/godotengine/godot-cpp godot-cpp
scons platform=windows arch=x86_64
# Esperado: bin\tamapoke_tray.dll criado
```

Sem o build, o jogo roda normalmente — só fica sem ícone na bandeja
(`_setup_tray` faz noop quando a extensão não existe).

## 3. Checklist de comportamento (Task 11, spec seção 7)

Com o jogo rodando (aba Remote quando precisar forçar stats):

1. Vagar desvia de água/árvores e às vezes cruza as pontes.
2. `PetStats.hunger = 10.0` → vai até as frutas, come (Eat), fome volta a 100.
3. `PetStats.energy = 10.0` → vai à casinha, dorme (Sleep), energia sobe ~8/s.
4. Treino: alterna vagar/descansar; ao cruzar o dojo acumula XP (balão).
5. Carinho: clique = pulso + corações; +2 XP; segundo clique em <30 s não soma.
6. `PetStats.hour_override = 23.0` → tint escurece e o bicho vai para a cama;
   acorda com `hour_override = 12.0`; com fome < 30 à noite acorda, come, volta.
7. Evolução: Remote `PetStats.add_xp(99999.0, "cheat")` → níveis; no 17 vira
   Charmeleon (sprite recarregado), no 37 Charizard (sem Eat, usa Idle).
8. Escala 0,5×/1×/2× pelo menu (botão direito); bicho proporcional; "Sempre no
   topo" liga/desliga.
9. F2 esconde a janela (processo vivo; reabrir só pela bandeja ou reiniciando o
   app — comportamento esperado até a bandeja existir). Bandeja: Mostrar/Esconder,
   Ver status, Sair; matar e reiniciar o explorer.exe recoloca o ícone.
10. Save: fechar (Sair/Alt+F4) e reabrir → estado mantido; com save antigo,
    bicho alimentado e XP offline (adiantando o relógio do PC +1 h antes de
    reabrir: fome/energia consistentes e XP +1..12).

Qualquer item falho: corrigir, re-testar e commitar antes de marcar.

## 4. Orientação das sprites (SkyTemple)

Confirmar visualmente a ordem das linhas dos sheets: linha 0 = Sul, sentido
horário até linha 7 = SO. Se as direções saírem espelhadas/rotacionadas, remapear
em `direction_for_velocity`/`ALL_DIRS` em `godot/scripts/pmd_sprite_loader.gd`.

## 5. Afinação do jardim (se necessário)

Se o bicho atravessar água/árvores ou não cruzar as pontes, ajustar os
`walkable_rects` e zonas em `godot/scenes/World.tscn` (Step 2 da Task 11).
Residual conhecido: pequenas sobreposições dos retângulos das pontes
(alvos exatos: `Rect2(460, 45, 160, 60)` / `Rect2(400, 825, 220, 55)`) e a
sobreposição de ~25×35 px entre EatZone/SleepZone perto de (950, 630).

## 6. Commit final da verificação

Depois de tudo verde e conferido:

```bash
git commit --allow-empty -m "test(godot): verificação manual do MVP completa

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```