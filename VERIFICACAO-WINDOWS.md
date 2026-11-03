# Verificação pendente — rodar no Windows

Branch: `claude/import-vale-termal` (cenário Vale Termal importado do PokePet +
Tasks 0–10 do plano `docs/superpowers/plans/2026-11-02-godot-sim-mvp.md`).

**Já verificado (no PC XEON, Godot 4.7.2.stable, Steam):**

- Suíte headless completa (`--import` + `run_all.gd`): **OK — todos os testes
  passaram**, inclusive em simulação de clone limpo (`.godot` apagado).
- Import do projeto sem erros — os únicos ERRORs restantes são da extensão da
  bandeja ainda não compilada (item 2 abaixo), esperados.

## 1. Build da extensão da bandeja (GDExtension)

No PowerShell (precisa de MSVC + SCons instalados):

```powershell
cd godot\src\tray
git clone --branch master https://github.com/godotengine/godot-cpp godot-cpp
scons platform=windows arch=x86_64
# Esperado: bin\tamapoke_tray.dll criado
```

Sem o build, o jogo roda normalmente — só fica sem ícone na bandeja
(`_setup_tray` faz noop quando a extensão não existe) e o `--import` imprime
os erros "GDExtension dynamic library not found".

## 2. Checklist de comportamento (Task 11, spec seção 7)

Com o jogo rodando (aba Remote quando precisar forçar stats). O cenário agora
é o Vale Termal: comedouro ~ (830, 675), clareira de treino ~ (915, 832),
cama/abrigo ~ (1020, 648), spawn (950, 723):

1. Vagar desvia do rio e dos objetos e às vezes cruza a ponte (x 444–634,
   y ~842) e a escada de pedra (pé em 872, 502).
2. `PetStats.hunger = 10.0` → vai ao comedouro, come (Eat), fome volta a 100.
3. `PetStats.energy = 10.0` → vai à cama, dorme (Sleep), energia sobe ~8/s.
4. Treino: na clareira de treino acumula XP (balão), energia desce.
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

## 3. Orientação das sprites (SkyTemple)

Confirmar visualmente a ordem das linhas dos sheets: linha 0 = Sul, sentido
horário até linha 7 = SO. Se as direções saírem espelhadas/rotacionadas, remapear
em `direction_for_velocity`/`ALL_DIRS` em `godot/scripts/pmd_sprite_loader.gd`.

## 4. Afinação do Vale Termal (se necessário)

Se o bicho atravessar água/objetos ou não alcançar os pontos, ajustar
`AGENT_RADIUS` em `godot/scripts/world.gd` (hoje 4 px — o spawn fica a 7 px do
banco e o comedouro a 6 px da mesa de frutas) ou as zonas em
`godot/scenes/World.tscn`. A navegação vem dos 111 polígonos do nó
`Habitat/Colisao` da cena `lar_do_charmander_aberta.tscn`; para mexer nas
colisões, edite no PokePet e reimporte.

## 5. Commit final da verificação

Depois de tudo verde e conferido:

```bash
git commit --allow-empty -m "test(godot): verificação manual do MVP completa

Co-Authored-By: Claude Code <noreply@anthropic.com>"
```