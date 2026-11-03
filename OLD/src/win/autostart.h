// src/win/autostart.h
#pragma once
// Autostart via HKCU\Software\Microsoft\Windows\CurrentVersion\Run.
// Todas as strings sao UTF-8 (convertidas para UTF-16 e gravadas com as APIs W).
// Argumentos nulos -> false.

// Grava (ou sobrescreve) o valor REG_SZ `valueName` = `commandLine`.
bool autostartEnable(const char* valueName, const char* commandLine);
// Remove o valor. Valor/chave inexistente conta como sucesso.
bool autostartDisable(const char* valueName);
// true se o valor existe e e REG_SZ.
bool autostartEnabled(const char* valueName);
