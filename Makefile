# Compilador y banderas de seguridad (Programación Defensiva)
CC = gcc
CFLAGS = -Wall

# El objetivo 'all' le dice a make que compile los 3 programas por defecto
all: launcher x11_keys IAlearner

# 1. Compilar el Gerente (Launcher)
launcher: launcher.c
	$(CC) $(CFLAGS) launcher.c -o launcher

# 2. Compilar el Cliente/Ventana X11 (Requiere la librería de interfaz gráfica)
x11_keys: x11_keys.c
	$(CC) $(CFLAGS) x11_keys.c -o x11_keys -lX11

# 3. Compilar el Servidor Remoto (Requiere Pthreads para la concurrencia)
IAlearner: IAlearner.c
	$(CC) $(CFLAGS) IAlearner.c -o IAlearner -pthread

# Comando para limpiar los ejecutables viejos (make clean)
clean:
	rm -f launcher x11_keys IAlearner
