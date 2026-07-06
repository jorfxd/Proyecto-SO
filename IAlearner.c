#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctype.h>
#include <strings.h>
#include <signal.h>

// ==============================================================================
// BLOQUE 1: ESTRUCTURAS DE DATOS Y CONFIGURACIÓN GLOBAL
// ==============================================================================

#define MAX_DICCIONARIOS 3
#define MAX_PALABRAS 20

typedef struct {
    char clase[50];
    char palabras[MAX_PALABRAS][50];
    int total_palabras;
} Diccionario;

Diccionario diccionarios[MAX_DICCIONARIOS];

// Estructura para el manejo seguro de estadísticas entre hilos
typedef struct {
    int correos;
    int cientificos;
    int reportes;
    int total_documentos;
    pthread_mutex_t lock; // Candado para evitar condiciones de carrera (Race Conditions)
} EstadisticasUsuario;

EstadisticasUsuario stats = {0, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER};

// Función comparadora para qsort y strcasecmp (ignora mayúsculas/minúsculas)
int comparar_palabras(const void *a, const void *b) {
    return strcasecmp((const char *)a, (const char *)b);
}

// ==============================================================================
// BLOQUE 2: CARGA Y PREPARACIÓN DE DICCIONARIOS (INTELIGENCIA)
// ==============================================================================

void cargar_diccionarios() {
    FILE *archivo = fopen("diccionarios.txt", "r");
    if (!archivo) {
        perror("[-] Error: No se encontró diccionarios.txt");
        exit(EXIT_FAILURE);
    }

    char linea[512];
    int i = 0;
    while (fgets(linea, sizeof(linea), archivo) && i < MAX_DICCIONARIOS) {
        linea[strcspn(linea, "\n")] = 0; 
        
        // Separamos clase de palabras
        char *clase = strtok(linea, ":");
        if (!clase) continue;
        strcpy(diccionarios[i].clase, clase);

        char *palabras_str = strtok(NULL, ":");
        if (!palabras_str) continue;
        
        // Purificación de palabras: eliminamos espacios, tabulaciones y retornos de carro
        char *palabra = strtok(palabras_str, ", \r\t");
        
        int j = 0;
        while (palabra != NULL && j < MAX_PALABRAS) {
            strcpy(diccionarios[i].palabras[j], palabra);
            palabra = strtok(NULL, ", \r\t"); 
            j++;
        }
        diccionarios[i].total_palabras = j;
        
        // Ordenamos para permitir búsqueda binaria eficiente
        qsort(diccionarios[i].palabras, diccionarios[i].total_palabras, 50, comparar_palabras);
        i++;
    }
    fclose(archivo);
}

// ==============================================================================
// BLOQUE 3: LÓGICA DE INFERENCIA Y MANEJO DE SEÑALES (PROGRAMACIÓN DEFENSIVA)
// ==============================================================================

void determinar_tipo_usuario() {
    pthread_mutex_lock(&stats.lock);
    int total = stats.total_documentos;
    
    if (total == 0) {
        printf("\n[!] No hay suficientes datos.\n");
        pthread_mutex_unlock(&stats.lock);
        return;
    }

    // Cálculo de proporciones (X) para clasificar al usuario
    float prop_correo = (float)stats.correos / total;
    float prop_cient = (float)stats.cientificos / total;
    float prop_reporte = (float)stats.reportes / total;

    printf("\n[IALearner] Análisis Final: Correo: %.2f, Científico: %.2f, Reporte: %.2f\n", prop_correo, prop_cient, prop_reporte);

    // Jerarquía de reglas según la rúbrica
    if (prop_correo > 0.5 && prop_reporte > 0.3) printf("[!] Usuario: Administrativo\n");
    else if (prop_correo > 0.4 && prop_reporte > 0.4) printf("[!] Usuario: Técnico\n");
    else if (prop_correo > 0.3 && prop_cient > 0.4) printf("[!] Usuario: Profesor\n");
    else printf("[!] Usuario: Estudiante\n");
    
    pthread_mutex_unlock(&stats.lock);
}

// Manejador de señales: captura Ctrl+C para realizar la inferencia final
void handle_sigint(int sig) {
    printf("\n[IALearner] Apagando servidor... ejecutando inferencia.\n");
    determinar_tipo_usuario();
    exit(0);
}

// ==============================================================================
// BLOQUE 4: PROCESAMIENTO CONCURRENTE (HILOS Y SOCKETS)
// ==============================================================================

void *atender_ventana(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char buffer[1024] = {0};
    char oracion[2048] = {0}; 
    char documento_completo[8192] = {0}; 
    char id_proceso[20] = {0}; 
    int valread;

    // A. Recolección de datos asíncrona
    while ((valread = read(sock, buffer, 1024)) > 0) {
        char tecla[50] = {0};
        if (sscanf(buffer, "P%[^:]: %s", id_proceso, tecla) == 2) {
            if (strcmp(tecla, "Return") == 0) {
                if (strlen(oracion) > 0) {
                    strcat(documento_completo, oracion);
                    strcat(documento_completo, " ");
                    memset(oracion, 0, sizeof(oracion));
                }
            } else if (strcmp(tecla, "space") == 0) strcat(oracion, " ");
            else if (strlen(tecla) == 1) strcat(oracion, tecla);
        }
        memset(buffer, 0, sizeof(buffer)); 
    }

    // B. Fase de Clasificación (Bag of Words)
    if (strlen(documento_completo) > 0) {
        int max_frecuencia_total = -1;
        int indice_ganador = -1;
        int frec_palabras[MAX_DICCIONARIOS][MAX_PALABRAS] = {0};

        char *token = strtok(documento_completo, " \n\r\t.,;:");
        while (token != NULL) {
            for (int i = 0; i < MAX_DICCIONARIOS; i++) {
                for (int j = 0; j < diccionarios[i].total_palabras; j++) {
                    if (strcasecmp(token, diccionarios[i].palabras[j]) == 0) {
                        frec_palabras[i][j]++;
                        break; 
                    }
                }
            }
            token = strtok(NULL, " \n\r\t.,;:");
        }

        // C. Aplicación de Reglas (Validación de 3 palabras y desempate por prioridad)
        for (int i = 0; i < MAX_DICCIONARIOS; i++) {
            int palabras_distintas = 0;
            int suma_frecuencia = 0;
            for (int j = 0; j < diccionarios[i].total_palabras; j++) {
                if (frec_palabras[i][j] > 0) {
                    palabras_distintas++;
                    suma_frecuencia += frec_palabras[i][j];
                }
            }

            if (palabras_distintas >= 3) {
                if (suma_frecuencia > max_frecuencia_total) {
                    max_frecuencia_total = suma_frecuencia;
                    indice_ganador = i;
                } else if (suma_frecuencia == max_frecuencia_total && i > indice_ganador) {
                    indice_ganador = i; // Desempate por jerarquía de prioridad
                }
            }
        }

        // D. Actualización segura del contador global
        if (indice_ganador != -1) {
            pthread_mutex_lock(&stats.lock);
            stats.total_documentos++;
            if (indice_ganador == 0) stats.correos++;
            else if (indice_ganador == 1) stats.cientificos++;
            else if (indice_ganador == 2) stats.reportes++;
            pthread_mutex_unlock(&stats.lock);
        }
    }

    close(sock);
    free(socket_desc);
    return NULL;
}

// ==============================================================================
// BLOQUE 5: INICIALIZACIÓN DEL SERVIDOR (MAIN)
// ==============================================================================

int main() {
    cargar_diccionarios();
    FILE *archivo = fopen("config.txt", "r");
    char puerto_str[20];
    fgets(puerto_str, sizeof(puerto_str), archivo);
    fclose(archivo);
    int puerto_config = atoi(puerto_str);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // Reciclaje inmediato

    signal(SIGINT, handle_sigint); // Registro de cierre seguro
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(puerto_config);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        pthread_t thread_id;
        int *new_sock = malloc(sizeof(int));
        *new_sock = new_socket;
        
        if (pthread_create(&thread_id, NULL, atender_ventana, (void*) new_sock) < 0) free(new_sock);
        pthread_detach(thread_id); // Liberación automática de recursos
    }
    return 0;
}