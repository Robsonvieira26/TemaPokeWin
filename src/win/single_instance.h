// src/win/single_instance.h
#pragma once
// Instancia unica por sessao, sem janela: um mutex nomeado "Local\<base>" marca a
// instancia primaria e um evento auto-reset "Local\<base>Show" leva o pedido
// "apareca" da instancia secundaria para a primaria. Sem threads.
class SingleInstance {
 public:
  SingleInstance() = default;
  ~SingleInstance() { release(); }
  SingleInstance(const SingleInstance&) = delete;
  SingleInstance& operator=(const SingleInstance&) = delete;

  // true  = esta e a unica instancia (ou este objeto ja a possui: chamada repetida
  //         devolve true sem sinalizar nada).
  // false = outra instancia ja roda: o evento "show" foi sinalizado e todos os
  //         handles locais foram fechados; o chamador deve sair.
  // `baseName` em UTF-8; testes passam um nome unico para nao esbarrar num app real.
  bool acquire(const char* baseName = "TamaPoke");

  // Nao bloqueia. true uma unica vez por pedido (varios pedidos pendentes se
  // fundem em um). So a instancia primaria ve pedidos; senao sempre false.
  bool showRequested();

  // Libera mutex e evento. Idempotente; o destrutor chama.
  void release();

 private:
  void* mutex_ = nullptr;  // HANDLE
  void* event_ = nullptr;  // HANDLE
};
