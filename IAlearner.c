#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // para concurrencia


// Función que ejecutará cada hilo para escuchar a una ventana específica
void *atender_ventana(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[1024] = {0};
    int valread;

    // Bucle infinito: lee todo lo que envía esta ventana
    while ((valread = read(sock, buffer, 1024)) > 0) {
        printf("[Data Center] Recibió: %s\n", buffer);
        memset(buffer, 0, sizeof(buffer)); // Limpiar buffer para la siguiente letra
    }

    printf("[Data Center] Una ventana se ha desconectado.\n");
    close(sock);
    free(socket_desc);
    return NULL;
}

int main() {
    FILE *archivo = fopen("config.txt", "r");
    if (archivo == NULL) {
        printf("[-] Error: No se encontró el archivo config.txt\n");
        exit(EXIT_FAILURE);
    }
    
    char puerto_str[20];
    fgets(puerto_str, sizeof(puerto_str), archivo);
    fclose(archivo);
    
    int puerto_config = atoi(puerto_str); // Convertir texto a número

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // 1. Crear el socket del servidor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket falló");
        exit(EXIT_FAILURE);
    }

    // ---> NUEVA DEFENSA: RECICLAJE INMEDIATO DEL PUERTO <---
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Error al configurar SO_REUSEADDR");
        exit(EXIT_FAILURE);
    } //termina

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Escucha en cualquier IP local
    address.sin_port = htons(puerto_config);

    // 2. Vincular el socket al puerto 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind falló");
        exit(EXIT_FAILURE);
    }

    // 3. Escuchar conexiones (hasta 10 ventanas en cola)
    if (listen(server_fd, 10) < 0) {
        perror("Listen falló");
        exit(EXIT_FAILURE);
    }

    printf("=========================================\n");
    printf("[IALearner] Data Center iniciado.\n");
    printf("[IALearner] Escuchando en el puerto %d...\n", puerto_config);
    printf("=========================================\n");

    // 4. Bucle infinito para aceptar ventanas (clientes)
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept falló");
            continue;
        }

        printf("\n[IALearner] [+] ¡Nueva ventana X11 conectada!\n");

        // Crear un hilo (Pthread) para que atienda a esta ventana concurrentemente
        pthread_t thread_id;
        int *new_sock = malloc(sizeof(int));
        *new_sock = new_socket;

        if (pthread_create(&thread_id, NULL, atender_ventana, (void*) new_sock) < 0) {
            perror("No se pudo crear el hilo");
            free(new_sock);
        }
        
        // Separar el hilo para que libere su memoria al terminar sin dejar zombies
        pthread_detach(thread_id);
    }

    return 0;
}
