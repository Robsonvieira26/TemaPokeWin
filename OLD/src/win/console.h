#pragma once
// Console de debug do desktop pet (F12 / --console). O exe e do subsistema WIN32 (sem console);
// aqui cria-se um com AllocConsole, religa stdout/stderr/stdin nele (a saida do firmware via HAL
// Serial aparece ali) e le comandos SEM bloquear (ReadConsoleInputW), montando linhas com o
// ConsoleLineEditor. Sem thread, sem std::cin.
//
// PROTECOES CONTRA MATAR O PROCESSO (um console criado por AllocConsole pertence ao processo):
//  * o comando Fechar (X) e removido do menu de sistema da janela do console;
//  * SetConsoleCtrlHandler: Ctrl+C e Ctrl+Break sao ignorados (retorna TRUE);
//  * CTRL_CLOSE/LOGOFF/SHUTDOWN chamam o callback registrado em consoleSetCloseCallback (o shell
//    pede saida limpa, que salva) e o handler espera ate ~4 s o processo terminar por conta
//    propria; retorna TRUE. O callback roda em OUTRA thread: so deve acionar um flag atomico.
// Todas as funcoes sao para a thread principal (exceto o callback acima).
#include <string>

// Abre se fechado, fecha se aberto. Retorna true se o console esta aberto depois da chamada.
// Nunca lanca; em falha (p.ex. o processo ja tem console) retorna false e nada muda.
bool consoleToggle();
bool consoleIsOpen();
// Nao bloqueante. true com uma linha completa (UTF-8, aparada, nao vazia). false se fechado.
bool consoleReadLine(std::string* out);
// Fecha (FreeConsole) se aberto; stdout/stderr/stdin voltam para NUL. Idempotente.
void consoleClose();
void consoleSetCloseCallback(void (*cb)());
