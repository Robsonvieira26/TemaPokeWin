// Stub inocuo da bandeja. A Task 21 substitui este arquivo pela implementacao real.
#include "win/tray.h"
#include "settings.h"

bool trayInit(TrayActions*, const Settings&) { return true; }
void trayShutdown() {}
void trayShowMenuAtCursor() {}
void traySetTooltip(const char*) {}
void traySyncChecks(const Settings&) {}
