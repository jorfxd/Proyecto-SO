#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // para concurrencia
#include <ctype.h> // NUEVA LIBRERÍA para manejo de caracteres
#include <strings.h>
#include <signal.h>


int comparar_palabras(const void *a, const void *b) {
    // strcasecmp compara dos textos ignorando si son mayúsculas o minúsculas
    return strcasecmp((const char *)a, (const char *)b);
}

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

typedef struct {
    int correos;
    int cientificos;
    int reportes;
    int total_documentos;
    pthread_mutex_t lock; // Candado para que los hilos no choquen
} EstadisticasUsuario;

EstadisticasUsuario stats = {0, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER};

// --- NUEVO: Función para imprimir y verificar la memoria de los diccionarios ---
void imprimir_diccionarios_cargados() {
    printf("\n=== RADIOGRAFÍA DE LOS DICCIONARIOS EN MEMORIA ===\n");
    for (int i = 0; i < MAX_DICCIONARIOS; i++) {
        // Solo imprimimos si la clase tiene un nombre válido
        if (strlen(diccionarios[i].clase) > 0) {
            printf("-> Clase: '%s' (Contiene %d palabras clave)\n", diccionarios[i].clase, diccionarios[i].total_palabras);
            printf("   Palabras en memoria: ");
            
            // Recorremos y mostramos cada palabra exacta que se guardó
            for (int j = 0; j < diccionarios[i].total_palabras; j++) {
                printf("[%s] ", diccionarios[i].palabras[j]);
            }
            printf("\n");
        }
    }
    printf("==================================================\n\n");
}

// --- NUEVO: Función para cargar diccionarios dinámicamente ---
void cargar_diccionarios() {
    FILE *archivo = fopen("diccionarios.txt", "r");
    if (!archivo) {
        perror("[-] Error: No se encontró diccionarios.txt");
        exit(EXIT_FAILURE); 
    }

    char linea[512];
    int i = 0;
    while (fgets(linea, sizeof(linea), archivo) && i < MAX_DICCIONARIOS) {
        linea[strcspn(linea, "\n")] = 0; // Limpiar salto de línea (\n)
        
        char *clase = strtok(linea, ":");
        if (!clase) continue; // Protección extra
        strcpy(diccionarios[i].clase, clase);

        char *palabras_str = strtok(NULL, ":");
        if (!palabras_str) continue;
        
        // ---> LA MAGIA ESTÁ AQUÍ <---
        // Agregamos un espacio en blanco ' ' y un retorno de carro '\r' a los delimitadores.
        // Esto purifica la palabra y elimina cualquier basura invisible del .txt
        char *palabra = strtok(palabras_str, ", \r\t");
        
        int j = 0;
        while (palabra != NULL && j < MAX_PALABRAS) {
            strcpy(diccionarios[i].palabras[j], palabra);
            
            // Usamos los mismos delimitadores estrictos para la siguiente palabra
            palabra = strtok(NULL, ", \r\t"); 
            j++;
        }
        diccionarios[i].total_palabras = j;
        
        // Ordenamos el diccionario alfabéticamente para la Búsqueda Binaria
        qsort(diccionarios[i].palabras, diccionarios[i].total_palabras, 50, comparar_palabras);
        
        i++;
    }
    fclose(archivo);
    printf("[+] Diccionarios cargados, LIMPIOS y ordenados exitosamente.\n");
}

void determinar_tipo_usuario() {
    pthread_mutex_lock(&stats.lock);
    int total = stats.total_documentos;
    
    if (total == 0) {
        printf("\n[!] No hay suficientes datos para inferir el tipo de usuario.\n");
        pthread_mutex_unlock(&stats.lock);
        return;
    }

    float prop_correo = (float)stats.correos / total;
    float prop_cient = (float)stats.cientificos / total;
    float prop_reporte = (float)stats.reportes / total;

    printf("\n=========================================\n");
    printf("[IALearner] Análisis Final de Usuario:\n");
    printf("Proporciones: Correo: %.2f, Científico: %.2f, Reporte: %.2f\n", prop_correo, prop_cient, prop_reporte);

    // Lógica de tabla de referencia (Ejemplo basado en tu rúbrica)
    if (prop_correo > 0.5 && prop_reporte > 0.3) {
        printf("[!] Usuario detectado: Personal administrativo\n");
    } else if (prop_correo > 0.4 && prop_reporte > 0.4) {
        printf("[!] Usuario detectado: Personal técnico\n");
    } else if (prop_correo > 0.3 && prop_cient > 0.4) {
        printf("[!] Usuario detectado: Profesor\n");
    } else {
        printf("[!] Usuario detectado: Estudiante\n");
    }
    printf("=========================================\n");
    
    pthread_mutex_unlock(&stats.lock);
}

void handle_sigint(int sig) {
    printf("\n[IALearner] Apagando servidor... ejecutando inferencia final.\n");
    determinar_tipo_usuario(); // <--- AQUÍ EJECUTAS LA INFERENCIA
    exit(0);
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
    printf("[Data Center] Ventana P%s cerrada. Iniciando análisis...\n", id_proceso);
    
    if (strlen(documento_completo) > 0) {
        int max_frecuencia_total = -1;
        int indice_ganador = -1;
        int frec_palabras[MAX_DICCIONARIOS][MAX_PALABRAS] = {0};

        printf("[DEBUG] Tokens extraídos del documento:\n");
        
        // PASO A: TOKENIZACIÓN CLÁSICA
        char *token = strtok(documento_completo, " \n\r\t.,;:");

        while (token != NULL) {
            printf("  -> Evaluando palabra: '%s'\n", token); // RAYOS X
            
            // PASO B: BÚSQUEDA ROBUSTA
            for (int i = 0; i < MAX_DICCIONARIOS; i++) {
                for (int j = 0; j < diccionarios[i].total_palabras; j++) {
                    if (strcasecmp(token, diccionarios[i].palabras[j]) == 0) {
                        printf("     [!] MATCH ENCONTRADO: '%s' pertenece a la clase '%s'\n", token, diccionarios[i].clase);
                        frec_palabras[i][j]++;
                        break; 
                    }
                }
            }
            token = strtok(NULL, " \n\r\t.,;:");
        }

        // PASO C: APLICAR REGLAS ESTRICTAS DE LA RÚBRICA
        for (int i = 0; i < MAX_DICCIONARIOS; i++) {
            int palabras_distintas = 0;
            int suma_frecuencia = 0;

            for (int j = 0; j < diccionarios[i].total_palabras; j++) {
                if (frec_palabras[i][j] > 0) {
                    palabras_distintas++;
                    suma_frecuencia += frec_palabras[i][j];
                }
            }

            // REGLA 1: Debe cumplir el mínimo de 3 palabras distintas
            if (palabras_distintas >= 3) {
                // REGLA 2: Lógica de Prioridad y Desempate
                if (suma_frecuencia > max_frecuencia_total) {
                    max_frecuencia_total = suma_frecuencia;
                    indice_ganador = i;
                } 
                // AQUÍ ESTÁ EL DESEMPATE POR PRIORIDAD:
                // Si la frecuencia es igual, mantenemos el que tiene mayor prioridad.
                // Asumimos que tu archivo diccionarios.txt está en orden: 
                // 0: Correo, 1: Científico, 2: Reporte.
                else if (suma_frecuencia == max_frecuencia_total && indice_ganador != -1) {
                    // Si el nuevo 'i' es mayor, tiene mayor prioridad (Reporte > Científico > Correo)
                    if (i > indice_ganador) {
                        indice_ganador = i;
                    }
                }
            }
        }

        if (indice_ganador != -1) {
            printf("[P%s] CLASIFICACIÓN: Este documento es de clase -> ** %s **\n", 
                   id_proceso, diccionarios[indice_ganador].clase);
		   pthread_mutex_lock(&stats.lock);
            	   stats.total_documentos++;
            	   if (indice_ganador == 0) stats.correos++;        // 0 = Correo
            	   else if (indice_ganador == 1) stats.cientificos++; // 1 = Científico
            	   else if (indice_ganador == 2) stats.reportes++;   // 2 = Reporte
            	   pthread_mutex_unlock(&stats.lock);
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
    cargar_diccionarios();
    imprimir_diccionarios_cargados();
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


    signal(SIGINT, handle_sigint);
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
