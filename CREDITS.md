# Creditos e licencas

TamaPoke para Windows e um projeto pessoal, **sem fins comerciais**, e **nao e afiliado**
nem endossado por Nintendo, Game Freak, Creatures ou The Pokemon Company. Pokemon e todos
os nomes, desenhos e personagens relacionados sao marcas registradas e direitos autorais
de seus respectivos donos.

## Codigo deste repositorio

`src/`, `hal/`, `tools/` e `tests/`: licenca MIT, ver [LICENSE](LICENSE).

O icone do aplicativo (`assets/tamapoke.ico`) e um recorte circular da tela do ovo,
desenhada proceduralmente pelo firmware (licenca MIT). Nao usa nenhum sprite de Pokemon.

## TamaPoke (firmware)

O jogo e o firmware original do TamaPoke (https://github.com/socquique/TamaPoke),
Copyright (c) 2026 Quique Tortosa, licenca MIT (`upstream/LICENSE`). Compilado aqui sem
alteracoes, como submodulo git em `upstream/`.

Conforme o proprio `upstream/LICENSE`, a licenca MIT cobre somente o codigo original do
firmware e das ferramentas; ela nao cobre os sprites nem os nomes de Pokemon.

## Sprites dos Pokemon

Os sprites sao de **PMD Sprite Collaboration** (https://github.com/PMDCollab/SpriteCollab),
obra de sua comunidade de artistas, sob **Creative Commons Atribuicao-NaoComercial 4.0
(CC BY-NC 4.0)**, conforme `upstream/CREDITS.md` e `upstream/LICENSE`. Os arquivos
empacotados (`sdcard/mons/*.bin`, copiados de `upstream/tools/sdcard/mons`) sao derivados
dessa fonte.

- **Uso apenas pessoal e nao comercial.** Nao redistribua os sprites comercialmente.
- Os creditos por especie e por autor estao no `tracker.json` do repositorio SpriteCollab
  (https://github.com/PMDCollab/SpriteCollab/blob/master/tracker.json).
- Os atributos base da 1a geracao vem da PokeAPI (https://pokeapi.co), segundo
  `upstream/CREDITS.md`.
- O `upstream/CREDITS.md` recomenda, ao publicar o projeto, distribuir apenas codigo e
  scripts e deixar cada usuario baixar e empacotar os sprites das fontes originais.

## Cenario Vale Termal (Godot)

O cenario do desktop pet (`godot/assets/sprites/erw/` e os tiles de
`godot/assets/ERW - Ancient Ruins V 2.3/`) usa o pacote de pixel art
**ERW - Ancient Ruins V 2.3** como base de tiles e props; os objetos derivados
(cabana, comedouro, saco de pancada, borboletas, variacoes de vegetacao etc.)
foram pintados sobre o estilo do pacote no projeto PokePet e seguem a licenca
do pacote original. Uso pessoal e nao comercial.

## Bibliotecas

| Componente | Licenca | Fonte da informacao |
|---|---|---|
| Arduino_GFX v1.6.9, de moononournation (`third_party/Arduino_GFX`) | BSD (Copyright (c) 2012 Adafruit Industries) | `third_party/Arduino_GFX/license.txt`; autoria em `upstream/CREDITS.md` |
| SDL3 (release-3.2.10), de Sam Lantinga e colaboradores | zlib, Copyright (C) 1997-2025 Sam Lantinga | `LICENSE.txt` do SDL3 baixado pelo CMake |
| nlohmann/json v3.11.3 | MIT, Copyright (c) 2013-2022 Niels Lohmann | `LICENSE.MIT` baixado pelo CMake |
| U8g2 2.37.1, de olikraus (somente os dados das fontes CJK, em `hal/u8g2_fonts.cpp`) | BSD de 2 clausulas, Copyright (c) 2016, olikraus@gmail.com | cabecalho de `hal/u8g2_fonts.cpp` |
| GNU Unifont (fontes `unifont_t_japanese3` e `unifont_t_korean2` dentro do U8g2) | SIL Open Font License 1.1 e GPLv2+ com a excecao GNU Font Embedding Exception | cabecalho de `hal/u8g2_fonts.cpp` |

GNU Unifont: Copyright (C) 1998-2024 Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew
Miller, Johnnie Weaver, David Corbett, Nils Moskopp, Rebecca Bettencourt, Ho-Seok Ee, et
al. (texto copiado do cabecalho de `hal/u8g2_fonts.cpp`).

Os textos completos das licencas de U8g2 e Unifont estao nos comentarios de
`hal/u8g2_fonts.cpp`; os demais, nos arquivos citados acima.
