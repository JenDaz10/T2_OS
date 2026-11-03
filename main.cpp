#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include "juez.h"
#include "servidor.h"
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

bool numero_positivo(const char *s,int &n) {
    if (!s || !*s) return false;
    char *fin=nullptr;
    errno=0;
    long v=strtol(s,&fin,10);
    if (errno || !fin || *fin || v<=0 || v>INT_MAX) return false;
    n=static_cast<int>(v); return true;
}

int main(int argc,char **argv) {
    if (argc<3 || argc>4) {
        std::cerr << "Uso: ./torneo N M [config.txt]\n";
        return 1;
    }
    int n=0,m=0;
    if (!numero_positivo(argv[1],n) || !numero_positivo(argv[2],m)) {
        std::cerr << "N y M deben ser enteros positivos.\n"; return 1;
    }
    Config cfg;
    if (argc==4) {
        std::string error;
        if (!cargar_config(argv[3],cfg,error)) {
            std::cerr << "Error: " << error << "\n"; return 1;
        }
    }
    try {
        Servidor s(n,m,cfg);
        sigemptyset(&s.senales);
        sigaddset(&s.senales,SIGINT);
        sigaddset(&s.senales,SIGUSR1);
        pthread_sigmask(SIG_BLOCK,&s.senales,nullptr);
        if (pthread_create(&s.hilo_senales,nullptr,esperar_senales,&s)!=0)
            throw std::runtime_error("No se pudo crear el controlador de senales");
        imprimir("[SERVIDOR] Inicio N="+std::to_string(n)+" clientes, M="+std::to_string(m)+" mesas.\n");
        for (int i=0;i<n;++i) {
            auto c=std::make_unique<Cliente>();
            c->servidor=&s;
            c->id=i;
            int rc=pthread_create(&c->hilo,nullptr,trabajo_cliente,c.get());
            if (rc!=0) {
                // Si falla la creacion, se detiene todo ordenadamente.
                solicitar_detencion(s);
                imprimir("[SERVIDOR] No se pudo crear cliente " + std::to_string(i)+"\n");
                break;
            }
            s.clientes.push_back(std::move(c));
        }
        int numero_mesa=0;
        for (;;) {
            int a=-1,b=-1;
            pthread_mutex_lock(&s.mutex);
            while (!s.detener && s.cola.size()<2 && s.llegados<n)
                pthread_cond_wait(&s.cv_llegadas,&s.mutex);
            if (s.detener) {
                pthread_mutex_unlock(&s.mutex); break;
            }
            if (s.cola.size()<2) {
                if (!s.cola.empty()) {
                    int suelto=s.cola.front();
                    s.cola.pop_front();
                    s.clientes[suelto]->completo=true;
                    pthread_cond_broadcast(&s.cv_clientes);
                    imprimir("[SERVIDOR] Cliente " + std::to_string(suelto) + " sin pareja (N impar).\n");
                }
                pthread_mutex_unlock(&s.mutex);
                break;
            }
            a=s.cola.front();s.cola.pop_front();
            b=s.cola.front();s.cola.pop_front();
            pthread_mutex_unlock(&s.mutex);
            imprimir("[SERVIDOR] Pareja "+std::to_string(a)+"/"+std::to_string(b)+
                    " lista; esperando mesa libre si M esta completo.\n");
            int espera;
            do { espera=sem_wait(&s.mesas); } while (espera==-1 && errno==EINTR);
            if (espera==-1) {
                imprimir("[SERVIDOR] Error sem_wait, terminando.\n");
                solicitar_detencion(s);
                break;
            }
            pthread_mutex_lock(&s.mutex);
            if (s.detener) { pthread_mutex_unlock(&s.mutex); break; }
            auto g=std::make_unique<Partida>(&s,++numero_mesa,a,b,cfg);
            Partida *ptr=g.get();
            s.partidas.push_back(std::move(g));
            s.clientes[a]->emparejado=true;
            s.clientes[a]->es_puma=true;
            s.clientes[a]->inicio_partida_ms=ahora_ms();
            s.clientes[b]->emparejado=true;
            s.clientes[b]->es_puma=false;
            s.clientes[b]->inicio_partida_ms=ahora_ms();
            pthread_cond_broadcast(&s.cv_clientes);

            //El SIGNIT no puede observar una partida sin hilo juez mientras solicita detenerla debido al mutex del servidor.
            int rc=pthread_create(&ptr->juez,nullptr,trabajo_juez,ptr);
            if (rc!=0) {
                s.partidas.pop_back();
                s.clientes[a]->completo=true;
                s.clientes[b]->completo=true;
                pthread_cond_broadcast(&s.cv_clientes);
                sem_post(&s.mesas);
                imprimir("[SERVIDOR] Error creando juez, pareja cancelada.\n");
            }
            pthread_mutex_unlock(&s.mutex);
        }

        // Todos los juegos se han creado o se ha pedido la interrupcion.
        for (auto &g:s.partidas) pthread_join(g->juez,nullptr);
        pthread_mutex_lock(&s.mutex);
        for (auto &c:s.clientes) c->completo=true;
        pthread_cond_broadcast(&s.cv_clientes);
        pthread_mutex_unlock(&s.mutex);
        for (auto &c:s.clientes) pthread_join(c->hilo,nullptr);
        pthread_kill(s.hilo_senales,SIGUSR1);
        pthread_join(s.hilo_senales,nullptr);
        imprimir("[SERVIDOR] Fin. Recursos POSIX liberados.\n");
    } catch (const std::exception &e) {
        std::cerr << "Error fatal: " << e.what() << "\n";return 1;
    }
    return 0;
}
