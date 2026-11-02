# TamaPoke para Windows

Port nativo para Windows, no formato desktop pet, do firmware ESP32 do
[TamaPoke](https://github.com/socquique/TamaPoke), sem modificar o upstream
(mantido como submodulo em `upstream/`).

Upstream fixado em: 1fff020c158282a384cc9b9357ada111e3eddba8

## Build

```bash
git submodule update --init
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

O executavel fica em `build/Debug/tamapoke.exe`.
