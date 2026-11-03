#include "juez.h"
#include <cassert>
#include <iostream>

int main() {
    Config cfg;
    cfg.puma_inicial=celda(2,2);
    Partida g(nullptr,1,0,1,cfg);
    std::string razon;
    assert(!g.validar({12,celda(2,2),celda(4,4),-1},razon));
    assert(g.grafo.vecinos(TRONO_TOP,TRONO_IZQ));
    assert(!g.grafo.vecinos(celda(2,4),TRONO_TOP));
    assert(!g.grafo.vecinos(celda(2,2),celda(3,3)));
    assert(g.grafo.vecinos(celda(0,2),TRONO_TOP));

    //Salto sobre un perro
    g.tablero.fill(VACIO);
    g.perros.fill(-1);
    g.puma=celda(2,2);
    g.perros[0]=celda(2,3);
    g.tablero[g.puma]=PUMA;
    g.tablero[g.perros[0]]=0;
    Movimiento salto{12,celda(2,2),celda(2,4),celda(2,3)};
    assert(g.validar(salto,razon));
    g.aplicar(salto);
    assert(g.capturas==1 && g.perros[0]==-1 && g.tablero[celda(2,4)]==PUMA);
    assert(!g.validar(salto,razon)); // origen obsoleto

    //Salto de 2 perros a la misma casilla, solo el 1ro la adquiere
    g.tablero.fill(VACIO);
    g.perros.fill(-1);
    g.puma=TRONO_TOP;
    g.tablero[g.puma]=PUMA;
    g.perros[0]=celda(2,1); g.tablero[celda(2,1)]=0;
    g.perros[1]=celda(3,2); g.tablero[celda(3,2)]=1;
    Movimiento a{0,celda(2,1),celda(2,2),-1};
    Movimiento b{1,celda(3,2),celda(2,2),-1};
    assert(g.validar(a,razon));
    assert(g.validar(b,razon));
    g.aplicar(a);
    assert(!g.validar(b,razon));
    assert(g.tablero[celda(2,2)]==0);

    //No hay saltos, casillas ocupadas, y el Puma no puede moverse a ninguna casilla vecina.
    g.tablero.fill(VACIO);
    g.perros.fill(-1);
    g.puma = celda(2, 2);
    g.tablero[g.puma] = PUMA;
    const int cerco[] = {
        celda(1,2), celda(3,2), celda(2,1), celda(2,3),
        celda(0,2), celda(4,2), celda(2,0), celda(2,4)
    };
    for (int i=0; i<8; ++i) {
        g.perros[i] = cerco[i];
        g.tablero[cerco[i]] = i;
    }
    assert(g.posibles(12).empty());
    std::cout << "OK: movimientos invalidos, captura, trono, colisiones y cerco del Puma\n";
    return 0;
}
