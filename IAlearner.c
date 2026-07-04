#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // para concurrencia
#include <ctype.h> // NUEVA LIBRERÍA para manejo de caracteres

// --- NUEVO: ESTRUCTURAS PARA BAG OF WORDS ---
#define MAX_DICCIONARIOS 3
#define MAX_PALABRAS 20

// Creamos el "molde" de lo que es un Diccionario
typedef struct {
    char clase[50];
    char palabras[MAX_PALABRAS][50];
    int total_palabras;
} Diccionario;

// Arreglo global para guardar los 3 diccionarios en memoria
Diccionario diccionarios[MAX_DICCIONARIOS];

// --- NUEVO: Función para cargar diccionarios dinámicamente ---
void cargar_diccionarios() {
    FILE *archivo = fopen("diccionarios.txt", "r");
    if (!archivo) {
        perror("[-] Error: No se encontró diccionarios.txt");
        exit(EXIT_FAILURE); // Si no hay archivo, el programa no debe seguir
    }

    char linea[512];
    int i = 0;
    while (fgets(linea, sizeof(linea), archivo) && i < MAX_DICCIONARIOS) {
        linea[strcspn(linea, "\n")] = 0; // Limpiar salto de línea (\n)
        
        // Extraemos el nombre de la clase (Ej: "Correo electrónico")
        char *clase = strtok(linea, ":");
        strcpy(diccionarios[i].clase, clase);

        // Extraemos las palabras separadas por comas
        char *palabras_str = strtok(NULL, ":");
        char *palabra = strtok(palabras_str, ",");
        
        int j = 0;
        while (palabra != NULL && j < MAX_PALABRAS) {
            strcpy(diccionarios[i].palabras[j], palabra);
            palabra = strtok(NULL, ","); // Siguiente palabra
            j++;
        }
        diccionarios[i].total_palabras = j;
        i++;
    }
    fclose(archivo);
    printf("[+] Diccionarios cargados exitosamente.\n");
}

// Función que ejecutará cada hilo para escuchar a una ventana específica
void *atender_ventana(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[1024] = {0};
    
    char oracion[2048] = {0}; 
    char documento_completo[8192] = {0}; // Almacena Todo lo que el usuario escribió
    char id_proceso[20] = {0}; 
    int valread;

    // --- 1. FASE DE RECOLECCIÓN ---
    while ((valread = read(sock, buffer, 1024)) > 0) {
        char tecla[50] = {0};
        
        // Extraemos quién envía y qué tecla es (Ej: "P1: A")
        if (sscanf(buffer, "P%[^:]: %s", id_proceso, tecla) == 2) {
            
            if (strcmp(tecla, "Return") == 0) {
                // Si aplasta Enter y la oración tiene texto, la guardamos
                if (strlen(oracion) > 0) {
		    printf("\n[Data Center] [P%s] Oración completada: %s\n", id_proceso, oracion);
                    strcat(documento_completo, oracion);
                    strcat(documento_completo, " "); // Añadimos espacio entre oraciones
                    memset(oracion, 0, sizeof(oracion)); // Limpiamos para la siguiente
                }
            } else if (strcmp(tecla, "space") == 0) {
                // Traducimos la tecla de espacio a un espacio real
                strcat(oracion, " ");
            } else if (strlen(tecla) == 1) {
                // Concatenamos las letras normales
                strcat(oracion, tecla);
            }
        }
        // Limpiamos el buffer de red
        memset(buffer, 0, sizeof(buffer)); 
    }

    // --- 2. FASE DE CLASIFICACIÓN (Bag of Words) ---
    // Esto se ejecuta únicamente cuando el usuario cierra la ventana gráfica
    printf("\n=========================================\n");
    printf("[Data Center] Ventana P%s cerrada. Iniciando análisis Bag of Words...\n", id_proceso);
    
    if (strlen(documento_completo) > 0) {
        int max_frecuencia_total = -1;
        int indice_ganador = -1;

        // Evaluamos el documento contra cada uno de los 3 diccionarios
        for (int i = 0; i < MAX_DICCIONARIOS; i++) {
            int palabras_distintas_encontradas = 0;
            int suma_frecuencia = 0;

            for (int j = 0; j < diccionarios[i].total_palabras; j++) {
                int coincidencias = 0;
                char *ptr = documento_completo;
                
                // strcasestr busca ignorando si es mayúscula o minúscula
                while ((ptr = strcasestr(ptr, diccionarios[i].palabras[j])) != NULL) {
                    coincidencias++;
                    ptr += strlen(diccionarios[i].palabras[j]); // Avanzar el puntero para seguir buscando
                }

                if (coincidencias > 0) {
                    palabras_distintas_encontradas++;
                    suma_frecuencia += coincidencias;
                }
            }

            // REGLA 1: Debe tener al menos 3 palabras distintas del diccionario
            if (palabras_distintas_encontradas >= 3) {
                // REGLA 2: Si hay empate, gana la clase con mayor frecuencia total
                if (suma_frecuencia > max_frecuencia_total) {
                    max_frecuencia_total = suma_frecuencia;
                    indice_ganador = i;
                }
            }
        }

        // Imprimir el veredicto final en la consola del Data Center
        if (indice_ganador != -1) {
            printf("[P%s] CLASIFICACIÓN: Este documento es de clase -> ** %s **\n", 
                   id_proceso, diccionarios[indice_ganador].clase);
        } else {
            printf("[P%s] CLASIFICACIÓN: Indeterminado (No cumple el mínimo de 3 palabras clave).\n", id_proceso);
        }
    } else {
        printf("[P%s] Documento vacío. Nada que analizar.\n", id_proceso);
    }
    printf("=========================================\n");

    // --- 3. LIMPIEZA DE MEMORIA Y RED ---
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
