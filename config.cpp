#include "config.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

Config::Config() { perros_iniciales.fill(-1); }

bool cargar_config(const std::string &archivo, Config &cfg, std::string &error) {
    std::ifstream in(archivo);
    if (!in) { error = "No se pudo abrir " + archivo; return false; }
    std::string linea;
    int num_linea = 0;
    while (std::getline(in, linea)) {
        ++num_linea;
        const size_t hash = linea.find('#');
        if (hash != std::string::npos) linea.resize(hash);
        std::istringstream s(linea);
        std::string clave;
        if (!(s >> clave)) continue;
        try {
            if (clave == "SEED") cfg.seed = [&]{ unsigned v; if (!(s >> v)) throw std::runtime_error("SEED"); return v; }();
            else if (clave == "DELAY_MIN") { if (!(s >> cfg.delay_min)) throw std::runtime_error("DELAY_MIN"); }
            else if (clave == "DELAY_MAX") { if (!(s >> cfg.delay_max)) throw std::runtime_error("DELAY_MAX"); }
            else if (clave == "MAX_MOVES") { if (!(s >> cfg.max_movimientos)) throw std::runtime_error("MAX_MOVES"); }
            else if (clave == "MAX_DURATION_MS") { if (!(s >> cfg.duracion_max_ms)) throw std::runtime_error("MAX_DURATION_MS"); }
            else if (clave == "LOG_EVERY") { if (!(s >> cfg.mostrar_cada)) throw std::runtime_error("LOG_EVERY"); }
            else if (clave == "PUMA_INIT") {
                int r, c; if (!(s >> r >> c) || celda(r,c) < 0) throw std::runtime_error("PUMA_INIT");
                cfg.puma_inicial = celda(r,c);
            } else if (clave == "DOG_INIT") {
                int perro, r, c;
                if (!(s >> perro >> r >> c) || perro < 0 || perro >= PERROS || celda(r,c) < 0)
                    throw std::runtime_error("DOG_INIT");
                cfg.perros_iniciales[perro] = celda(r,c);
            } else if (clave == "DISCONNECT_PLAYER") {
                if (!(s >> cfg.desconectar) ||
                    (cfg.desconectar != "PUMA" && cfg.desconectar != "PERROS" && cfg.desconectar != "NINGUNO"))
                    throw std::runtime_error("DISCONNECT_PLAYER");
            } else if (clave == "DISCONNECT_AFTER_MS") {
                if (!(s >> cfg.desconectar_tras_ms)) throw std::runtime_error("DISCONNECT_AFTER_MS");
            } else {
                throw std::runtime_error("Clave desconocida: " + clave);
            }
        } catch (const std::exception &e) {
            error = "Configuracion, linea " + std::to_string(num_linea) + ": " + e.what();
            return false;
        }
    }
    if (cfg.delay_min < 1 || cfg.delay_max < cfg.delay_min || cfg.delay_max > 60000 ||
        cfg.max_movimientos < 1 || cfg.duracion_max_ms < 1 || cfg.mostrar_cada < 1 ||
        cfg.desconectar_tras_ms < -1) {
        error = "Valores invalidos de tiempos, limite de movimientos o LOG_EVERY";
        return false;
    }
    std::array<bool, CASILLAS> usados{};
    usados[cfg.puma_inicial] = true;
    for (int p : cfg.perros_iniciales) {
        if (p < 0) continue;
        if (usados[p]) { error = "Piezas iniciales superpuestas"; return false; }
        usados[p] = true;
    }
    return true;
}
