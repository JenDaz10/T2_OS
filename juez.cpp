#include "juez.h"
#include "servidor.h"
#include <memory>
#include <random>
#include <sstream>
#include <utility>

Partida::Partida(Servidor *s,int id,int a,int b,const Config &c) :
    servidor(s),numero(id),cliente_puma(a),cliente_perros(b),cfg(c),puma(c.puma_inicial) {
    pthread_mutex_init(&mutex,nullptr);
    pthread_cond_init(&cv_peticiones,nullptr);
    pthread_cond_init(&cv_respuestas,nullptr);
    tablero.fill(VACIO);
    tablero[puma] = PUMA;
    perros.fill(-1);
    std::array<bool,CASILLAS> ocupadas{};
    ocupadas[puma] = true;
    for (int i=0;i<PERROS;++i) {
        int pos=cfg.perros_iniciales[i];
        if (pos>=0) { perros[i]=pos; ocupadas[pos]=true; tablero[pos]=i; }
    }

    for (int i=0;i<PERROS;++i) if (perros[i]<0) {
        for (int k=0;k<25;++k) if (!ocupadas[k]) {
            perros[i]=k; tablero[k]=i; ocupadas[k]=true; break;
        }
    }
}
Partida::~Partida() {
    pthread_cond_destroy(&cv_peticiones);
    pthread_cond_destroy(&cv_respuestas);
    pthread_mutex_destroy(&mutex);
}

std::vector<Movimiento> Partida::posibles(int pieza) const {
    std::vector<Movimiento> movs;
    const int origen = pieza==12 ? puma : perros[pieza];
    if (origen<0) return movs;
    for (int d : grafo.ady[origen]) {
        if (tablero[d]==VACIO) movs.push_back({pieza,origen,d,-1});
        else if (pieza==12 && tablero[d]>=0) {
            for (int fin : grafo.ady[d]) {
                if (fin!=origen && tablero[fin]==VACIO && grafo.recta(origen,d,fin))
                    movs.push_back({pieza,origen,fin,d});
            }
        }
    }
    return movs;
}

bool Partida::validar(const Movimiento &m, std::string &motivo) const {
    if (m.pieza<0 || m.pieza>12 || m.hasta<0 || m.hasta>=CASILLAS ||
        m.desde<0 || m.desde>=CASILLAS) { motivo="fuera de rango"; return false; }
    int actual = m.pieza==12 ? puma : perros[m.pieza];
    if (actual != m.desde || actual<0) { motivo="origen obsoleto o ficha capturada"; return false; }
    if (tablero[m.hasta]!=VACIO) { motivo="destino ocupado"; return false; }
    if (m.sobre==-1) {
        if (!grafo.vecinos(m.desde,m.hasta)) { motivo="no son vecinos"; return false; }
    } else {
        if (m.pieza!=12 || m.sobre<0 || m.sobre>=CASILLAS ||
            tablero[m.sobre]<0 || !grafo.vecinos(m.desde,m.sobre) ||
            !grafo.vecinos(m.sobre,m.hasta) || !grafo.recta(m.desde,m.sobre,m.hasta)) {
            motivo="captura invalida"; return false;
        }
    }
    return true;
}

void Partida::aplicar(const Movimiento &m) {
    tablero[m.desde] = VACIO;
    if (m.sobre!=-1) {
        int id=tablero[m.sobre];
        tablero[m.sobre]=VACIO;
        perros[id]=-1;
        ++capturas;
    }
    tablero[m.hasta] = m.pieza==12 ? PUMA : m.pieza;
    if (m.pieza==12) puma=m.hasta;
    else perros[m.pieza]=m.hasta;
    ++aceptados;
}

std::string Partida::mostrar_tablero() const {
    auto marca=[&](int n)->char {
        if (n==VACIO) return '.';
        if (n==PUMA) return 'P';
        return 'D';
    };
    std::ostringstream out;
    out << "       " << marca(tablero[TRONO_TOP]) << "         [T]\n";
    out << "     " << marca(tablero[TRONO_IZQ]) << "---" << marca(tablero[TRONO_DER]) << "       [trono]\n";
    for (int r=0;r<5;++r) {
        out << "   ";
        for (int c=0;c<5;++c) {
            out << marca(tablero[celda(r,c)]) << (c==4 ? '\n' : ' ');
        }
    }
    out << "   P=Puma, D=Perro, .=Vacio; capturas=" << capturas << "\n";
    return out.str();
}

void Partida::finalizar(const std::string &causa) {
    if (terminada) return;
    terminada=true;
    resultado=causa;
    pthread_cond_broadcast(&cv_respuestas);
    pthread_cond_broadcast(&cv_peticiones);
}

struct ArgumentoFicha { Partida *partida; int pieza; };

void *trabajo_ficha(void *dato) {
    std::unique_ptr<ArgumentoFicha> a(static_cast<ArgumentoFicha *>(dato));
    Partida &g=*a->partida;
    int idx=a->pieza;
    std::mt19937 rng(g.cfg.seed ^ (unsigned(g.numero)*2654435761U) ^ (unsigned(idx+5)*1013904223U));
    std::uniform_int_distribution<int> tiempo(g.cfg.delay_min,g.cfg.delay_max);
    for (;;) {
        dormir_ms(tiempo(rng));
        pthread_mutex_lock(&g.mutex);
        while (g.pendiente[idx] && !g.terminada && !g.cancelar)
            pthread_cond_wait(&g.cv_respuestas,&g.mutex);
        if (g.terminada || g.cancelar || (idx<12 && g.perros[idx]<0)) {
            pthread_mutex_unlock(&g.mutex); break;
        }
        auto candidatos=g.posibles(idx);
        if (!candidatos.empty()) {
            if (idx==12) {
                std::vector<Movimiento> capturas;
                for (const auto &mov:candidatos) if (mov.sobre!=-1) capturas.push_back(mov);
                if (!capturas.empty()) candidatos=std::move(capturas);
            }
            std::uniform_int_distribution<size_t> escoger(0,candidatos.size()-1);
            g.peticiones.push_back(candidatos[escoger(rng)]);
            g.pendiente[idx]=true;
            pthread_cond_signal(&g.cv_peticiones);
        }
        pthread_mutex_unlock(&g.mutex);
    }
    return nullptr;
}

void terminar_clientes(Servidor &s, int a, int b) {
    pthread_mutex_lock(&s.mutex);
    s.clientes[a]->completo=true;
    s.clientes[b]->completo=true;
    pthread_cond_broadcast(&s.cv_clientes);
    pthread_mutex_unlock(&s.mutex);
}

void *trabajo_juez(void *dato) {
    Partida &g=*static_cast<Partida *>(dato);
    Servidor &s=*g.servidor;
    imprimir("\n[Mesa " + std::to_string(g.numero) + "] Juez inicia: cliente " +
            std::to_string(g.cliente_puma) + "=Puma, cliente " +
            std::to_string(g.cliente_perros) + "=Perros\n" + g.mostrar_tablero());
    g.inicio_ms=ahora_ms();
    pthread_mutex_lock(&g.mutex);
    if (g.posibles(12).empty()) g.finalizar("PERROS: Puma sin movimientos al inicio");
    pthread_mutex_unlock(&g.mutex);
    for (int i=0;i<13;++i) {
        pthread_mutex_lock(&g.mutex);
        bool parar=g.terminada || g.cancelar;
        pthread_mutex_unlock(&g.mutex);
        if (parar) break;
        auto *arg=new ArgumentoFicha{&g,i};
        int rc=pthread_create(&g.fichas[i],nullptr,trabajo_ficha,arg);
        if (rc!=0) {
            delete arg;
            pthread_mutex_lock(&g.mutex);
            g.finalizar("ABORTADA: fallo creando hilo de ficha");
            pthread_mutex_unlock(&g.mutex);
            break;
        }
        ++g.fichas_creadas;
    }
    for (;;) {
        std::string evento, estado;
        bool debe_imprimir=false;
        pthread_mutex_lock(&g.mutex);
        // El juez observa el estado de las conexiones, no es el propio juez
        // quien fabrica el abandono. Los clientes pueden terminar/ser cancelados.
        if (s.clientes[g.cliente_puma]->desconectado.load() && !g.terminada)
            g.finalizar("PERROS ganan por abandono del cliente Puma");
        else if (s.clientes[g.cliente_perros]->desconectado.load() && !g.terminada)
            g.finalizar("PUMA gana por abandono del cliente Perros");
        if (g.cancelar) g.finalizar("CANCELADA por SIGINT o cierre del servidor");
        if (ahora_ms()-g.inicio_ms>=g.cfg.duracion_max_ms && !g.terminada)
            g.finalizar("EMPATE: limite de tiempo");
        if (g.aceptados>=g.cfg.max_movimientos && !g.terminada)
            g.finalizar("EMPATE: limite de movimientos");
        if (g.terminada) {
            pthread_cond_broadcast(&g.cv_respuestas);
            pthread_mutex_unlock(&g.mutex);
            break;
        }
        if (g.peticiones.empty()) {
            timespec limite=plazo_ms(60);
            pthread_cond_timedwait(&g.cv_peticiones,&g.mutex,&limite);
            pthread_mutex_unlock(&g.mutex);
            continue;
        }
        Movimiento req=g.peticiones.front();
        g.peticiones.pop_front();
        g.pendiente[req.pieza]=false;
        pthread_cond_broadcast(&g.cv_respuestas);
        std::string error;
        if (g.validar(req,error)) {
            g.aplicar(req);
            evento="[Mesa " +std::to_string(g.numero)+"] "+(req.pieza==12?"Puma":"Perro "+std::to_string(req.pieza))+
                " " +nombre_celda(req.desde)+" -> "+nombre_celda(req.hasta);
            if (req.sobre>=0) evento+=" [captura en "+nombre_celda(req.sobre)+"]";
            evento+="\n";
            if (g.capturas>=6) g.finalizar("PUMA gana: capturo 6 perros");
            else if (g.posibles(12).empty()) g.finalizar("PERROS ganan: Puma acorralado");
            debe_imprimir=true;
            if (g.aceptados%g.cfg.mostrar_cada==0 || g.terminada)
                estado=g.mostrar_tablero();
        } else {
            // Las jugadas quedaron obsoletas por movimientos de otros hilos.
            evento="[Mesa "+std::to_string(g.numero)+"] RECHAZADO "+
                (req.pieza==12?"Puma":"Perro "+std::to_string(req.pieza))+
                " -> "+nombre_celda(req.hasta)+": "+error+"\n";
            debe_imprimir=true;
        }
        pthread_mutex_unlock(&g.mutex);
        if (debe_imprimir) imprimir(evento+estado);
    }
    for (int i=0;i<g.fichas_creadas;++i) pthread_join(g.fichas[i],nullptr);
    imprimir("[Mesa "+std::to_string(g.numero)+"] FINAL: "+g.resultado+
            " (movimientos="+std::to_string(g.aceptados)+", capturas="+std::to_string(g.capturas)+")\n");
    terminar_clientes(s,g.cliente_puma,g.cliente_perros);
    sem_post(&s.mesas);
    return nullptr;
}
