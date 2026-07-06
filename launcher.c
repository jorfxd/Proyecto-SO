#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>      // Para llamadas al sistema: fork(), exec(), kill()
#include <sys/wait.h>    // Para waitpid() (limpieza de procesos)
#include <signal.h>      // Para manejo de señales (SIGTERM, SIGINT)

// ==============================================================================
// BLOQUE 1: FUNCIONES DE AYUDA Y GESTIÓN DE PROCESOS
// ==============================================================================

// Muestra el menú interactivo para el usuario
void mostrar_info() {
    printf("\n--- Comandos Disponibles ---\n");
    printf("* 'nuevo (numero)'  : Abre N ventanas de captura X11.\n");
    printf("* 'estado'          : Lista procesos hijos activos.\n");
    printf("* 'cerrar'          : Cierra una ventana por su PID.\n");
    printf("* 'salir'           : Cierra todo el entorno y termina.\n");
    printf("* 'colapsar'        : Cierra todas las ventanas X11 activas.\n");
    printf("* 'info'            : Muestra este menú.\n");
    printf("----------------------------\n");
}

// Bloque de Limpieza: usa pgrep para filtrar procesos por nombre y los termina
void cerrar_todo() {
    printf("\n[!] Iniciando cierre masivo de ventanas X11...\n");
    // pgrep -f busca por nombre, xargs pasa los PIDs encontrados al comando kill
    if (system("pgrep -f x11_keys | xargs -r kill") == 0) {
        printf("[+] Señal de cierre enviada a todas las ventanas.\n");
    } else {
        printf("[-] No hay ventanas activas para cerrar.\n");
    }
}

// Manejador de señales: garantiza que al presionar Ctrl+C no queden procesos huérfanos
void manejo_cierre_abrupto(int sig) {
    printf("\n[!] Alerta: Cerrando Agentic Jorge por señal externa.\n");
    kill(0, SIGTERM); // Envía señal a todo el grupo de procesos
    exit(0);
}

// ==============================================================================
// BLOQUE 2: BUCLE PRINCIPAL Y GESTIÓN DEL SISTEMA
// ==============================================================================

int main() {
    // Registro del manejador de señales para un cierre seguro ante Ctrl+C
    signal(SIGINT, manejo_cierre_abrupto); 
    
    char comando[100];
    pid_t pid; 
    int contador_ventanas = 0;

    printf("Hola, este es la interfaz de Agentic Jorge :)\n");
    mostrar_info();

    while (1) {
        // [DEFENSA] Limpiador silencioso (Non-blocking wait):
        // Revisa constantemente si algún hijo terminó para liberar sus recursos y evitar zombies.
        while (waitpid(-1, NULL, WNOHANG) > 0);

        printf("\nAgenticJorge> ");
        if (fgets(comando, sizeof(comando), stdin) == NULL) break;
        comando[strcspn(comando, "\n")] = 0; // Limpiamos el salto de línea

        // --- A. CREACIÓN DE PROCESOS (Comando "nuevo") ---
        if (strncmp(comando, "nuevo", 5) == 0) {
            int cantidad_a_crear = 0;
            // Validamos que el usuario ingrese la cantidad: "nuevo 5"
            if (sscanf(comando, "nuevo %d", &cantidad_a_crear) != 1 || cantidad_a_crear <= 0) {
                printf("[-] Error: Debe especificar cantidad. Ejemplo: 'nuevo 1'\n");
                continue;
            }

            for (int i = 0; i < cantidad_a_crear; i++) {
                contador_ventanas++;
                char id_str[20];
                sprintf(id_str, "%d", contador_ventanas);

                pid = fork(); // Clonación del proceso

                if (pid < 0) { // [DEFENSA] Fallo de sistema (recursos agotados)
                    perror("[-] Error: Fallo en fork()");
                    contador_ventanas--;
                    break;
                } else if (pid == 0) { // [Hijo] Transformación a ventana gráfica
                    execl("./x11_keys", "./x11_keys", id_str, NULL);
                    perror("[-] Error al ejecutar x11_keys"); // Solo se llega aquí si exec falla
                    exit(1);
                } else { // [Padre] Monitoreo
                    printf("[+] Ventana 'proceso%d' creada con PID: %d\n", contador_ventanas, pid);
                }
            }
        }
        // --- B. LISTADO DE ESTADO ---
        else if (strcmp(comando, "estado") == 0) {
            printf("\n[+] Procesos activos:\n");
            system("pgrep -a x11_keys");
        }
        // --- C. CIERRE ESPECÍFICO ---
        else if (strcmp(comando, "cerrar") == 0) {
            char input_pid[20];
            printf(">> Ingresa el PID a cerrar: ");
            if (fgets(input_pid, sizeof(input_pid), stdin) != NULL) {
                pid_t pid_a_cerrar = atoi(input_pid);
                if (pid_a_cerrar > 0 && kill(pid_a_cerrar, SIGTERM) == 0) {
                    printf("[+] Proceso %d terminado.\n", pid_a_cerrar);
                } else {
                    printf("[-] PID inválido o inexistente.\n");
                }
            }
        }
        // --- D. CIERRE MASIVO Y SALIDA ---
        else if (strcmp(comando, "colapsar") == 0) {
            cerrar_todo();
        }
        else if (strcmp(comando, "salir") == 0) {
            printf("Saliendo de Agentic Jorge... ¡Adiós!\n");
            kill(0, SIGTERM); // Barre con todos los hijos antes de salir
            break;
        }
        else if (strlen(comando) > 0) {
            printf("[-] Comando inválido. Escriba 'info' para ayuda.\n");
        }
    }
    return 0;
}