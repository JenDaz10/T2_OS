#define _POSIX_C_SOURCE 200809L
#include "servidor.h"
#include "juez.h"
#include <cerrno>
#include <iostream>
#include <random>
#include <stdexcept>

// Mutex para evitar que las impresiones de distintas mesas se entremezclen.
pthread_mutex_t salida_mutex = PTHREAD_MUTEX_INITIALIZER;

void imprimir(const std::string &mensaje) {
    pthread_mutex_lock(&salida_mutex);
    std::cout << mensaje << std::flush;
    pthread_mutex_unlock(&salida_mutex);
}

void dormir_ms(int ms) {
    if (ms <= 0) return;
    timespec t{ms / 1000, (ms % 1000) * 1000000L};
    while (nanosleep(&t, &t) == -1 && errno == EINTR) {}
}

timespec plazo_ms(int ms) {
    timespec t{};
    clock_gettime(CLOCK_REALTIME, &t);
    t.tv_sec += ms / 1000;
    t.tv_nsec += (ms % 1000) * 1000000L;
    if (t.tv_nsec >= 1000000000L) { ++t.tv_sec; t.tv_nsec -= 1000000000L; }
    return t;
}

long long ahora_ms() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000LL + t.tv_nsec / 1000000LL;
}

Servidor::Servidor(int cantidad,int mesas_max,Config configuracion):cfg(configuracion),n(cantidad),m(mesas_max) {
    pthread_mutex_init(&mutex,nullptr);
    pthread_cond_init(&cv_llegadas,nullptr);
    pthread_cond_init(&cv_clientes,nullptr);
    if (sem_init(&mesas,0,static_cast<unsigned>(m))!=0)
        throw std::runtime_error("sem_init fallo");
}
Servidor::~Servidor() {
    partidas.clear();
    sem_destroy(&mesas);
    pthread_cond_destroy(&cv_llegadas);
    pthread_cond_destroy(&cv_clientes);
    pthread_mutex_destroy(&mutex);
}

void limpieza_cliente_cancelado(void *dato) {
    Cliente &c=*static_cast<Cliente *>(dato);
    c.desconectado.store(true);
    pthread_mutex_unlock(&c.servidor->mutex);
}

void *trabajo_cliente(void *dato) {
    Cliente &c=*static_cast<Cliente *>(dato);
    Servidor &s=*c.servidor;
    std::mt19937 rng(s.cfg.seed + unsigned(c.id)*911U);
    std::uniform_int_distribution<int> demora(0,80);
    dormir_ms(demora(rng));
    pthread_mutex_lock(&s.mutex);
    pthread_cleanup_push(limpieza_cliente_cancelado,&c);
    ++s.llegados;
    if (!s.detener) s.cola.push_back(c.id);
    else c.completo=true;
    pthread_cond_broadcast(&s.cv_llegadas);
    while (!c.completo && !s.detener) {
        bool es_objetivo=(c.es_puma && s.cfg.desconectar=="PUMA") ||
                         (!c.es_puma && s.cfg.desconectar=="PERROS");
        if (c.emparejado && es_objetivo && s.cfg.desconectar_tras_ms>=0) {
            long long falta=s.cfg.desconectar_tras_ms-(ahora_ms()-c.inicio_partida_ms);
            if (falta<=0) {
                c.desconectado.store(true);
                break; // El juez observa el estado y asigna la victoria.
            }
            timespec t=plazo_ms(static_cast<int>(falta));
            pthread_cond_timedwait(&s.cv_clientes,&s.mutex,&t);
        } else {
            pthread_cond_wait(&s.cv_clientes,&s.mutex);
        }
    }
    pthread_cleanup_pop(0);
    pthread_mutex_unlock(&s.mutex);
    return nullptr;
}

void solicitar_detencion(Servidor &s) {
    pthread_mutex_lock(&s.mutex);
    if (s.detener) { pthread_mutex_unlock(&s.mutex); return; }
    s.detener=true;
    pthread_cond_broadcast(&s.cv_llegadas);
    pthread_cond_broadcast(&s.cv_clientes);
    for (auto &p:s.partidas) {
        pthread_mutex_lock(&p->mutex);
        p->cancelar=true;
        pthread_cond_broadcast(&p->cv_peticiones);
        pthread_cond_broadcast(&p->cv_respuestas);
        pthread_mutex_unlock(&p->mutex);
    }
    pthread_mutex_unlock(&s.mutex);
    sem_post(&s.mesas);
    imprimir("\n[SERVIDOR] Detencion solicitada: cerrando clientes y mesas.\n");
}

void *esperar_senales(void *dato) {
    Servidor &s=*static_cast<Servidor *>(dato);
    int sig=0;
    while (sigwait(&s.senales,&sig)==0) {
        if (sig==SIGUSR1) break; // Aviso interno para cerrar el hilo al finalizar.
        if (sig==SIGINT) { solicitar_detencion(s); break; }
    }
    return nullptr;
}
