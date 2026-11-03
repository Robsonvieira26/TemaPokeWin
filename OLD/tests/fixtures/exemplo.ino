// Exemplo para gen_protos: comentario com { ( " e ' soltos
/* bloco com { e ( e "
   em varias linhas */
#include <Wire.h>
#include "fake.h"   // FakeType viria deste header
#define MACRO(x) { (x) }
#define LONGA(a, \
  b) do { a; b; } while (0)

struct Early { int a; };
int counter = 0;
int first(Early e) {                 // primeira funcao
  const char *s = "}{ ( \" ";
  if (e.a) { for (;;) { char c = '{'; (void)c; break; } }
  return s[0];
}
struct Late { int v; };
struct { int x, y; } anon = { 1, 2 };
static const int TABLE[][2] = { {1,2}, {3,4} };
int (*fp)(int) = nullptr;
auto lam = [](int v) { return v + 1; };
static bool helper(const Late &l, int n) { return l.v > n; }
void IRAM_ATTR isr() { counter++; }
FakeType useFake(FakeType t) { return t; }
uint16_t
multiLine(int a,
          const char *b,   // comentario ( com parentese
          Late *l)
{
  return a;
}
void over(int a) {}
void over(const char *s) {}
void declared();
void declared() {}
