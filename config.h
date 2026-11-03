#pragma once
#include "tablero.h"
#include <array>
#include <string>

struct Config {
    unsigned seed = 42;
    int delay_min = 50;
    int delay_max = 300;
    int max_movimientos = 700;
    int duracion_max_ms = 15000;
    int mostrar_cada = 15;
    int puma_inicial = celda(2, 2);
    std::array<int, PERROS> perros_iniciales{};
    std::string desconectar = "NINGUNO"; // PUMA, PERROS o NINGUNO (simulacion)
    int desconectar_tras_ms = -1;
    Config();
};

bool cargar_config(const std::string &archivo, Config &cfg, std::string &error);
