#include "tablero.h"
#include <algorithm>

int celda(int fila, int columna) {
    if (fila < 0 || fila >= 5 || columna < 0 || columna >= 5) return -1;
    return fila * 5 + columna;
}

std::string nombre_celda(int id) {
    if (id == TRONO_TOP) return "TRONO_TOP";
    if (id == TRONO_IZQ) return "TRONO_LEFT";
    if (id == TRONO_DER) return "TRONO_RIGHT";
    return "(" + std::to_string(id / 5) + "," + std::to_string(id % 5) + ")";
}

Grafo::Grafo() {
        for (int r = 0; r < 5; ++r) for (int c = 0; c < 5; ++c) {
            int u = celda(r,c); x[u] = c; y[u] = r;
            if (r > 0) arista(u, celda(r-1,c));
            if (c > 0) arista(u, celda(r,c-1));
        }
        x[TRONO_IZQ] = 1; y[TRONO_IZQ] = -1;
        x[TRONO_DER] = 3; y[TRONO_DER] = -1;
        x[TRONO_TOP] = 2; y[TRONO_TOP] = -2;
        arista(TRONO_TOP, TRONO_IZQ);
        arista(TRONO_TOP, TRONO_DER);
        arista(TRONO_TOP, celda(0,2));
        arista(TRONO_IZQ, TRONO_DER);
        arista(TRONO_IZQ, celda(0,0));
        arista(TRONO_IZQ, celda(0,1));
        arista(TRONO_DER, celda(0,3));
        arista(TRONO_DER, celda(0,4));
    }
void Grafo::arista(int a,int b) { ady[a].push_back(b); ady[b].push_back(a); }
bool Grafo::vecinos(int a,int b) const {
        if (a < 0 || a >= CASILLAS || b < 0 || b >= CASILLAS) return false;
        auto &v = ady[a]; return std::find(v.begin(),v.end(),b) != v.end();
    }
bool Grafo::recta(int a,int b,int c) const {
        // El destino debe continuar exactamente en la misma direccion del salto.
        return x[c] - x[b] == x[b] - x[a] && y[c] - y[b] == y[b] - y[a];
    }
