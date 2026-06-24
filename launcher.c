#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>      // Para fork(), exec(), pid_t
#include <sys/wait.h>    // Para waitpid() y limpieza de zombies

// Función para mostrar el menú de ayuda
void mostrar_info() {
    printf("\n--- Comandos Disponibles ---\n");
    printf("* 'nuevo'  : Abre una nueva ventana de captura X11 para Agentic OS.\n");
    printf("* 'estado' : Muestra el estado actual y cuántas ventanas están activas.\n");
    printf("* 'cerrar' : Cierra una ventana específica de captura.\n");
    printf("* 'salir'  : Cierra el entorno, envía la señal de fin al IALearner y termina.\n");
    printf("* 'info'   : Muestra este menú de ayuda nuevamente.\n");
    printf("----------------------------\n");
}

int main() {
    char comando[100];
    pid_t pid; // Variable para guardar el ID del proceso

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

        if (strcmp(comando, "info") == 0) {
            mostrar_info();
            
        } else if (strcmp(comando, "nuevo") == 0) {
            printf(">> Ejecutando: Abriendo nueva ventana secreta...\n");
            
            // 1. Clonamos el proceso launcher
            pid = fork(); 

            if (pid < 0) {
                // Error al crear el proceso
                perror("Error: No se pudo crear el proceso hijo");
                
            } else if (pid == 0) {
                // 2. ESTE ES EL CLON (Proceso Hijo)
                // Usamos execl para reemplazar el clon con tu programa X11 compilado
                // Asegúrate de que el ejecutable "x11_keys" esté en la misma carpeta.
                execl("./x11_keys", "./x11_keys", NULL);
                
                // Si execl falla (por ejemplo, si no encuentra el archivo), el código sigue aquí:
                perror("Error al ejecutar x11_keys. ¿Olvidaste compilarlo con gcc?");
                exit(1); // El hijo debe morir si falla
            }
            // 3. ESTE ES EL PADRE (Launcher)
            // El padre no entra al 'else if (pid == 0)'. Simplemente sigue su camino
            // en el ciclo while y vuelve a pedir un comando.
            
        } else if (strcmp(comando, "estado") == 0) {
            printf(">> Ejecutando: Mostrando ventanas activas...\n");
            // (Pendiente para más adelante)
            
        } else if (strcmp(comando, "cerrar") == 0) {
            printf(">> Ejecutando: Preparando para cerrar ventana...\n");
            // (Pendiente para más adelante)
            
        } else if (strcmp(comando, "salir") == 0) {
            printf(">> Cerrando Agentic Jorge. ¡Adiós!\n");
            break;
            
        } else {
            printf("Debe de ingresar una palabra correcta, consulte 'info' para proseguir.\n");
        }
    }

    return 0;
}
