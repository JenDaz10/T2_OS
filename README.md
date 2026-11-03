# Torneo concurrente de Komikan (C++17)

Tarea 2 de Sistemas Operativos. Simula un torneo de Komikan con jugadores y mesas concurrentes usando hilos y mecanismos de sincronización POSIX. Se ejecuta en Linux, incluido Ubuntu mediante WSL.

## Compilación

Desde la carpeta del proyecto:

```bash
make
```

O se puede compilar manualmente:

```bash
g++ -Wall -Wextra -std=c++17 -pthread main.cpp config.cpp tablero.cpp juez.cpp servidor.cpp -o torneo
```

## Ejecución

```bash
./torneo 4 2
./torneo 20 2 config.txt
./torneo 3 1 config.txt
```

El programa recibe `N` (cantidad de clientes), `M` (máximo de mesas simultáneas) y, opcionalmente, un archivo de configuración. Los clientes se agrupan en parejas: uno representa al Puma y otro a los Perros. Si queda un cliente sin pareja, se informa al finalizar el emparejamiento.

Para guardar la salida en un archivo y consultar solo los eventos principales:

```bash
./torneo 20 2 > resultado20.txt 2>&1
grep -E "SERVIDOR|Juez inicia|FINAL" resultado20.txt
```

Para detener el torneo durante su ejecución se puede utilizar `Ctrl+C`.

## Organización de archivos

| Archivos | Función |
|---|---|
| `main.cpp` | Recibe los argumentos e inicia el servidor. |
| `config.h`, `config.cpp` | Lee las opciones de configuración y comprueba que sean válidas. |
| `tablero.h`, `tablero.cpp` | Representa las 25 casillas y las 3 posiciones del trono, junto con sus conexiones. |
| `juez.h`, `juez.cpp` | Administra cada partida, valida los movimientos y comprueba las condiciones de victoria. |
| `servidor.h`, `servidor.cpp` | Organiza las parejas, administra las mesas y coordina la detención del torneo. |
| `Makefile` | Permite compilar el programa y las pruebas. |
| `pruebas.cpp`, `verificar.py` | Contiene pruebas de movimientos, concurrencia y cierre del programa. |
| `test_cases.txt` | Contiene escenarios de prueba proporcionados para la tarea. |

## Sincronización

- **Semáforo contador (`sem_t`):** limita la cantidad de mesas activas a `M`. Cuando no hay cupos disponibles, las nuevas parejas esperan hasta que termine una partida.
- **Mutex (`pthread_mutex_t`):** protege la información que comparten los hilos, como la cola de clientes y el estado del tablero. Evita que dos movimientos modifiquen una misma partida al mismo tiempo.
- **Variables de condición (`pthread_cond_t`):** permiten esperar la llegada de clientes, solicitudes de movimiento o respuestas del juez sin consultar continuamente.
- **Juez de mesa:** cada partida tiene un hilo juez independiente. Las fichas envían sus solicitudes y el juez las revisa antes de actualizar el tablero.
- **Señales (`sigwait`):** se utiliza un hilo para recibir `SIGINT` (`Ctrl+C`), solicitar la detención de las partidas y coordinar el cierre de los recursos.

## Configuración

El archivo de configuración es opcional. Por ejemplo, `config.txt` puede contener:

```text
SEED 42
DELAY_MIN 50
DELAY_MAX 300
PUMA_INIT 2 2
MAX_MOVES 700
MAX_DURATION_MS 15000
LOG_EVERY 15
```

`SEED` indica la semilla aleatoria; `DELAY_MIN` y `DELAY_MAX` definen los retardos entre intentos de movimiento en milisegundos; `PUMA_INIT` indica la posición inicial del Puma. Los otros parámetros controlan los límites de duración, cantidad de movimientos y frecuencia de impresión del tablero.

También se puede usar `DOG_INIT` para cambiar la posición inicial de un Perro, indicando su identificador (de 0 a 11), fila y columna. Para probar abandonos simulados se utilizan `DISCONNECT_PLAYER` y `DISCONNECT_AFTER_MS`.

La semilla permite repetir las decisiones aleatorias, aunque el orden de ejecución de los hilos puede variar entre ejecuciones.

## Pruebas

```bash
make test
python3 verificar.py
```

Las pruebas revisan movimientos inválidos, capturas, movimientos dentro del trono, colisiones entre fichas, victoria por cerco, límite de mesas, número impar de clientes, abandonos simulados y detención con `Ctrl+C`.

El archivo `test_cases.txt` contiene escenarios de referencia, pero **no se ejecuta directamente** como archivo de configuración: el programa no interpreta sus instrucciones `EVENT`.

## Alcance

Los clientes se simulan mediante hilos dentro de un mismo proceso; no hay conexiones de red. Los abandonos se simulan de forma controlada. La recuperación ante la caída completa del proceso no está implementada.
