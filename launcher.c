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
        if (strcmp(comando, "nuevo") == 0) {
            contador_ventanas++; // Sumamos 1 a la cuenta
            
            // Convertimos el número a texto porque execl solo acepta textos (strings)
            char id_str[20];
            sprintf(id_str, "%d", contador_ventanas); 

            pid = fork(); // Clonamos el proceso

            if (pid < 0) {
                perror("[-] Error: No se pudo hacer fork");
            } else if (pid == 0) {
                // SOMOS EL HIJO: Nos transformamos en la ventana y le pasamos el id_str
                // Nota: Asegúrate de que el ejecutable de tu ventana se llame "x11_keys"
                execl("./x11_keys", "./x11_keys", id_str, NULL);
                
                // Si execl falla, imprimimos error y salimos para no dejar basura
                perror("[-] Error al ejecutar x11_keys (¿Ya lo compilaste?)");
                exit(1);
            } else {
                // SOMOS EL PADRE (Launcher): Avisamos que todo salió bien
                printf("[+] Se ha creado la ventana 'proceso%d' con PID: %d\n", contador_ventanas, pid);
            }
        } 
        else if (strcmp(comando, "info") == 0) {
            mostrar_info();
        }
        else if (strcmp(comando, "estado") == 0) {
            printf("[!] El comando 'estado' está en construcción...\n");
        }
        else if (strcmp(comando, "cerrar") == 0) {
            printf("[!] El comando 'cerrar' está en construcción...\n");
        }
        else if (strcmp(comando, "salir") == 0) {
            printf("Saliendo de Agentic Jorge... ¡Hasta pronto!\n");
	    kill(0,SIGTERM);
            break;
        }
        else if (strlen(comando) > 0) {
            // Si el usuario escribe algo que no existe
            printf("[-] Comando no reconocido. Consulte 'info' para proseguir.\n");
        }
    }
    
    return 0;
}
