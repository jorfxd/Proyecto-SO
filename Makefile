# Compilador a usar
CC = gcc

# Banderas de compilación (warnings habilitados para programación defensiva)
CFLAGS = -Wall

# Librerías necesarias
LIBS = -lX11

# La regla 'all' es la que se ejecuta por defecto al escribir 'make'
all: launcher x11_keys

# Regla para compilar el launcher (El Padre)
launcher: launcher.c
	$(CC) $(CFLAGS) launcher.c -o launcher

# Regla para compilar la ventana gráfica de IBM (El Hijo)
x11_keys: x11_keys.c
	$(CC) $(CFLAGS) x11_keys.c -o x11_keys $(LIBS)

# Regla para limpiar los ejecutables viejos
clean:
	rm -f launcher x11_keys
