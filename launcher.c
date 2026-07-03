#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>      // Para fork(), exec(), pid_t
#include <sys/wait.h>    // Para waitpid() y limpieza de zombies

// Función para mostrar el menú de ayuda
void mostrar_info() {
    printf("\n--- Comandos Disponibles ---\n");
    printf("* 'nuevo (numero)'  : Abre nuevas ventanas de captura X11 para Agentic OS.\n");
    printf("* 'estado' : Muestra el estado actual y cuántas ventanas están activas.\n");
    printf("* 'cerrar' : Cierra una ventana específica de captura.\n");
    printf("* 'salir'  : Cierra el entorno, envía la señal de fin al IALearner y termina.\n");
    printf("* 'colapsar'   : cierra todas las ventanas.\n");
    printf("* 'info'   : Muestra este menú de ayuda nuevamente.\n");
    printf("----------------------------\n");
}

void cerrar_todo() {
    printf("\n[!] Iniciando cierre masivo de ventanas X11...\n");
    
    // Usamos 'pkill' pero excluyendo nuestro propio PID
    char comando_kill[100];
    sprintf(comando_kill, "pgrep -f x11_keys | xargs -r kill"); 
    
    // pgrep -f x11_keys busca solo los procesos de tus ventanas
    // xargs -r kill toma esos PIDs y los mata.
    if (system(comando_kill) == 0) {
        printf("[+] Todas las ventanas X11 han sido cerradas.\n");
    } else {
        printf("[-] No hay ventanas activas para cerrar.\n");
    }
}


void manejo_cierre_abrupto(int sig) {
    printf("\n[!] Alerta: Cerrando Agentic Jorge forzosamente. Limpiando ventanas huérfanas...\n");
    // kill(0, SIGTERM) envía la señal de terminación a todos los procesos hijos de este grupo
    kill(0, SIGTERM); 
    exit(0);
}

int main() {
    signal(SIGINT, manejo_cierre_abrupto); // Atrapa el Ctrl+C
    char comando[100];
    pid_t pid; // Variable para guardar el ID del proceso
    int contador_ventanas = 0; // NUEVO: Llevaremos la cuenta de cuántas ventanas creamos

    printf("Hola este es la interaz de Agentic Jorge:)\n");
    mostrar_info();

    while (1) {
        // [DEFENSA] Limpiador silencioso de procesos zombies:
        // Revisa si alguna ventana X11 ya se cerró y libera su memoria.
        while (waitpid(-1, NULL, WNOHANG) > 0);

        printf("\nAgenticJorge> ");

        if (fgets(comando, sizeof(comando), stdin) == NULL) {
            break; 
        }
        comando[strcspn(comando, "\n")] = 0;

        // --- LÓGICA DE COMANDOS ---
        if (strncmp(comando, "nuevo", 5) == 0) {
            
            int cantidad_a_crear = 0; // Inicializamos en 0 por seguridad
            
            // Intentamos escanear el número directamente
            int elementos_leidos = sscanf(comando, "nuevo %d", &cantidad_a_crear);
            
            // DEFENSA ESTRICTA: Si no logró leer 1 elemento (el número) o es <= 0
            if (elementos_leidos != 1 || cantidad_a_crear <= 0) {
                printf("[-] Error: Formato incompleto o incorrecto.\n");
                printf("    Ahora es OBLIGATORIO especificar la cantidad de ventanas.\n");
                printf("    Ejemplo de uso correcto: nuevo 1  (o también: nuevo 5)\n");
                continue; // Regresa al inicio del bucle pidiendo un nuevo comando
            }

            // Bucle para crear las N ventanas solicitadas
            for (int i = 0; i < cantidad_a_crear; i++) {
                contador_ventanas++; 
                
                char id_str[20];
                sprintf(id_str, "%d", contador_ventanas); 

                pid = fork(); 

                // ---> DEFENSA NATIVA: Ubuntu rechaza la creación (-1) <---
                if (pid < 0) {
                    perror("[-] Error Crítico del SO: fork() devolvió -1 (Recursos agotados)");
                    printf("    Se aborta la creación de las %d ventanas restantes para no colapsar.\n", (cantidad_a_crear - i));
                    contador_ventanas--; // Revertimos la cuenta de esta ventana fallida
                    break; // ROMPE EL BUCLE FOR
                } 
                else if (pid == 0) {
                    // SOMOS EL HIJO
                    execl("./x11_keys", "./x11_keys", id_str, NULL);
                    perror("[-] Error al ejecutar x11_keys");
                    exit(1);
                } 
                else {
                    // SOMOS EL PADRE
                    printf("[+] Se ha creado la ventana 'proceso%d' con PID: %d\n", contador_ventanas, pid);
                }
            }
        }
        else if (strcmp(comando, "info") == 0) {
            mostrar_info();
        }
        else if (strcmp(comando, "estado") == 0) {
            printf("\n=== Estado de Agentic OS ===\n");
            printf("[+] Ventanas X11 (Clientes) en ejecución:\n");
            
            // imprime los procesos activos '-a' hace que imprima el PID y también el número de proceso que le pasamos
            int estado_pgrep = system("pgrep -a x11_keys");
            
            // Si pgrep no encuentra nada, devuelve un código de error distinto de 0
            if (estado_pgrep != 0) {
                printf("    [-] Ninguna ventana activa en este momento.\n");
            }
            printf("============================\n");
        }
        else if (strcmp(comando, "cerrar") == 0) {
            char input_pid[20];
            printf(">> Ingresa el PID de la ventana X11 que deseas cerrar: ");
            
            // Leemos el PID que el usuario escribe
            if (fgets(input_pid, sizeof(input_pid), stdin) != NULL) {
                // Limpiamos el salto de línea como hicimos con el comando principal
                input_pid[strcspn(input_pid, "\n")] = 0;
                
                // Convertimos el texto ingresado a un número entero (pid_t)
                pid_t pid_a_cerrar = atoi(input_pid);
                
                if (pid_a_cerrar > 0) {
                    // Enviamos la señal SIGTERM para cerrarlo amablemente
                    if (kill(pid_a_cerrar, SIGTERM) == 0) {
                        printf("[+] Señal de cierre enviada exitosamente al proceso PID: %d.\n", pid_a_cerrar);
                    } else {
                        // Si pones un PID que no existe, te avisa el error
                        perror("[-] Error al intentar cerrar el proceso");
                    }
                } else {
                    printf("[-] PID inválido. Debe ser un número mayor a 0.\n");
                }
            }
        }
	else if (strcmp(comando, "colapsar") == 0) {
	    cerrar_todo();
	}
        else if (strcmp(comando, "salir") == 0) {
            printf("Saliendo de Agentic Jorge... ¡Hasta pronto!\n");
	    kill(0, SIGTERM);
            break;
        }
        else if (strlen(comando) > 0) {
            // Si el usuario escribe algo que no existe
            printf("[-] Comando no reconocido. Consulte 'info' para proseguir.\n");
        }
    }
    
    return 0;
}
