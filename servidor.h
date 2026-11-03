#pragma once
#include "config.h"
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <atomic>
#include <deque>
#include <memory>
#include <string>
#include <vector>

struct Partida;
struct Servidor;

void imprimir(const std::string &mensaje);
void dormir_ms(int ms);
timespec plazo_ms(int ms);
long long ahora_ms();

struct Cliente {
    Servidor *servidor;
    int id;
    pthread_t hilo{};
    bool completo = false;      // protegido por servidor->mutex
    bool emparejado = false;    // protegido por servidor->mutex
    bool es_puma = false;       // protegido por servidor->mutex
    long long inicio_partida_ms = 0;
    std::atomic<bool> desconectado{false};
};

struct Servidor {
    Config cfg;
    int n, m;
    pthread_mutex_t mutex{};
    pthread_cond_t cv_llegadas{}, cv_clientes{};
    sem_t mesas{};
    std::deque<int> cola;
    std::vector<std::unique_ptr<Cliente>> clientes;
    std::vector<std::unique_ptr<Partida>> partidas;
    int llegados = 0;
    bool detener = false;
    pthread_t hilo_senales{};
    sigset_t senales{};

    Servidor(int cantidad, int mesas_max, Config configuracion);
    ~Servidor();
};

void *trabajo_cliente(void *dato);
void solicitar_detencion(Servidor &s);
void *esperar_senales(void *dato);
