CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -pthread -O2
MODULOS = config.cpp tablero.cpp juez.cpp servidor.cpp
FUENTES = main.cpp $(MODULOS)
CABECERAS = config.h tablero.h juez.h servidor.h

all: torneo

torneo: $(FUENTES) $(CABECERAS)
	$(CXX) $(CXXFLAGS) $(FUENTES) -o torneo

pruebas: pruebas.cpp $(MODULOS) $(CABECERAS)
	$(CXX) $(CXXFLAGS) pruebas.cpp $(MODULOS) -o pruebas

test: pruebas
	./pruebas

clean:
	rm -f torneo pruebas *.o

.PHONY: all test clean
