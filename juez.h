#pragma once
#include "config.h"
#include "tablero.h"
#include <pthread.h>
#include <array>
#include <deque>
#include <string>
#include <vector>

struct Servidor;

// Todo el estado de una partida se comparte entre el Juez y las 13 fichas.
struct Partida {
    Servidor *servidor;
    int numero, cliente_puma, cliente_perros;
    Config cfg;
    Grafo grafo;
    std::array<int, CASILLAS> tablero{};
    std::array<int, PERROS> perros{};
    int puma, capturas = 0, aceptados = 0;
    bool terminada = false;
    bool cancelar = false;
    std::string resultado;
    pthread_mutex_t mutex{};
    pthread_cond_t cv_peticiones{}, cv_respuestas{};
    std::deque<Movimiento> peticiones;
    std::array<bool, 13> pendiente{};
    std::array<pthread_t, 13> fichas{};
    int fichas_creadas = 0;
    pthread_t juez{};
    long long inicio_ms = 0;

    Partida(Servidor *s, int id, int a, int b, const Config &c);
    ~Partida();
    std::vector<Movimiento> posibles(int pieza) const;
    bool validar(const Movimiento &m, std::string &motivo) const;
    void aplicar(const Movimiento &m);
    std::string mostrar_tablero() const;
    void finalizar(const std::string &causa);
};

void *trabajo_juez(void *dato);
