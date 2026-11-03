#pragma once
#include <array>
#include <string>
#include <vector>

// Cada casilla se representa como un nodo: 25 del tablero y 3 del trono.
constexpr int CASILLAS = 28;
constexpr int PERROS = 12;
constexpr int PUMA = -2;
constexpr int VACIO = -1;
constexpr int TRONO_IZQ = 25;
constexpr int TRONO_DER = 26;
constexpr int TRONO_TOP = 27;

int celda(int fila, int columna);
std::string nombre_celda(int id);

struct Grafo {
    std::array<std::vector<int>, CASILLAS> ady;
    std::array<int, CASILLAS> x{}, y{};

    Grafo();
    void arista(int a, int b);
    bool vecinos(int a, int b) const;
    bool recta(int a, int b, int c) const;
};

struct Movimiento {
    int pieza;
    int desde;
    int hasta;
    int sobre = -1; // Casilla del Perro capturado, -1 si no hay captura
};
