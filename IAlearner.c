#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // para concurrencia
#include <ctype.h>   // manejo de caracteres
#include <strings.h>
#include <signal.h>

// PARA LAS P ORACIONEES
pthread_mutex_t mutex_queue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_loader = PTHREAD_COND_INITIALIZER; // Despierta al Loader o a los detectores
pthread_cond_t cond_detectores = PTHREAD_COND_INITIALIZER;
int P_param; // Parámetro P ingresado por consola

// COLAS DE ORACIONES COMPARTIDAS
#define MAX_QUEUE_SIZE 1000
typedef struct
{
    char sentence[256];
    int client_id; // Para saber a qué contexto/computador pertenece
} SentenceNode;
SentenceNode sentence_queue[MAX_QUEUE_SIZE];
int queue_count = 0;

// NORMAL
int comparar_palabras(const void *a, const void *b)
{
    // strcasecmp compara dos textos ignorando si son mayúsculas o minúsculas
    return strcasecmp((const char *)a, (const char *)b);
}

// --- NUEVO: ESTRUCTURAS PARA BAG OF WORDS ---
#define MAX_DICCIONARIOS 3
#define MAX_PALABRAS 20

// DICCIONARIO
typedef struct
{
    char clase[50];
    char palabras[MAX_PALABRAS][50];
    int total_palabras;
} Diccionario;
Diccionario diccionarios[MAX_DICCIONARIOS];

// USUARIO
typedef struct
{
    int correos;
    int cientificos;
    int reportes;
    int total_documentos;
    pthread_mutex_t lock; // Candado para que los hilos no choquen
} EstadisticasUsuario;
EstadisticasUsuario stats = {0, 0, 0, 0, PTHREAD_MUTEX_INITIALIZER};

// --- HILO LOADER: Espera a que haya P oraciones y da la orden ---
void *hilo_loader(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&mutex_queue);

        // Mientras la cola tenga menos de P oraciones, el Loader duerme
        while (queue_count < P_param)
        {
            pthread_cond_wait(&cond_loader, &mutex_queue);
        }

        // Cuando se alcanzan P oraciones, despierta a los P detectores en paralelo
        pthread_cond_broadcast(&cond_detectores);

        pthread_mutex_unlock(&mutex_queue);
        usleep(100000); // Pequeño respiro
    }
    return NULL;
}

// --- POOL DE HILOS DETECTORES: Procesan en paralelo el Bag of Words ---
void *hilo_detector(void *arg)
{
    char mi_oracion[256];
    int mi_client_id;

    while (1)
    {
        pthread_mutex_lock(&mutex_queue);

        // Los hilos detectores esperan a que el Loader los reactive
        while (queue_count < P_param)
        {
            pthread_cond_wait(&cond_detectores, &mutex_queue);
        }

        // Sacan una oración de la cola compartida
        queue_count--;
        strcpy(mi_oracion, sentence_queue[queue_count].sentence);
        mi_client_id = sentence_queue[queue_count].client_id;

        pthread_mutex_unlock(&mutex_queue);

        // --- FASE DE CLASIFICACIÓN (Bag of Words por Oración) ---
        if (strlen(mi_oracion) > 0)
        {
            // RETROALDIRECCIÓN EN PANTALLA: Avisamos que el hilo comenzó el análisis
            printf("\n=========================================\n");
            printf("[Detector Thread] [P%d] Analizando oración: \"%s\"\n", mi_client_id, mi_oracion);
            printf("[DEBUG] Tokens extraídos de la oración:\n");

            int max_frecuencia_total = -1;
            int indice_ganador = -1;
            int frec_palabras[MAX_DICCIONARIOS][MAX_PALABRAS] = {0};

            // PASO A: TOKENIZACIÓN CLÁSICA (usando mi_oracion)
            char *token = strtok(mi_oracion, " \n\r\t.,;:");

            while (token != NULL)
            {
                // Mostramos cada palabra evaluada (Tus Rayos X)
                printf("  -> Evaluando palabra: '%s'\n", token);

                // PASO B: BÚSQUEDA ROBUSTA (bsearch)
                for (int i = 0; i < MAX_DICCIONARIOS; i++)
                {
                    char *encontrado = (char *)bsearch(
                        token,
                        diccionarios[i].palabras,
                        diccionarios[i].total_palabras,
                        sizeof(diccionarios[i].palabras[0]),
                        comparar_palabras);

                    if (encontrado != NULL)
                    {
                        // Retroalimentación de match exacto
                        printf("    [!] MATCH ENCONTRADO: '%s' pertenece a la clase '%s'\n", token, diccionarios[i].clase);

                        int j = (encontrado - (char *)diccionarios[i].palabras) / sizeof(diccionarios[i].palabras[0]);
                        frec_palabras[i][j]++;
                        break; // Salimos al hallar coincidencia
                    }
                }
                token = strtok(NULL, " \n\r\t.,;:");
            }

            // PASO C: APLICAR REGLAS ESTRICTAS DE LA RÚBRICA
            for (int i = 0; i < MAX_DICCIONARIOS; i++)
            {
                int palabras_distintas = 0;
                int suma_frecuencia = 0;

                for (int j = 0; j < diccionarios[i].total_palabras; j++)
                {
                    if (frec_palabras[i][j] > 0)
                    {
                        palabras_distintas++;
                        suma_frecuencia += frec_palabras[i][j];
                    }
                }

                // REGLA 1: Mínimo 3 palabras distintas
                if (palabras_distintas >= 3)
                {
                    // REGLA 2: Lógica de Prioridad y Desempate
                    if (suma_frecuencia > max_frecuencia_total)
                    {
                        max_frecuencia_total = suma_frecuencia;
                        indice_ganador = i;
                    }
                    else if (suma_frecuencia == max_frecuencia_total && indice_ganador != -1)
                    {
                        if (i > indice_ganador)
                        {
                            indice_ganador = i;
                        }
                    }
                }
            }

            // Sección crítica para actualizar las estadísticas globales de forma segura
            pthread_mutex_lock(&stats.lock);
            stats.total_documentos++;
            if (indice_ganador != -1)
            {
                printf("[Detector Thread] [P%d] CLASIFICACIÓN: Oración de clase -> ** %s **\n",
                       mi_client_id, diccionarios[indice_ganador].clase);

                if (indice_ganador == 0)
                    stats.correos++;
                else if (indice_ganador == 1)
                    stats.cientificos++;
                else if (indice_ganador == 2)
                    stats.reportes++;
            }
            else
            {
                printf("[Detector Thread] [P%d] CLASIFICACIÓN: Indeterminado (< 3 palabras clave).\n", mi_client_id);
            }
            pthread_mutex_unlock(&stats.lock);
            printf("=========================================\n");

            // --- NUEVO REQUERIMIENTO (e): Inferencia de usuario asincrónica en tiempo real ---
            determinar_tipo_usuario();
        }
        else
        {
            printf("[Detector Thread] [P%d] Oración vacía. Nada que analizar.\n", mi_client_id);
        }
    }
    return NULL;
}

// imprimir y verificar la memoria de los diccionarios=
void imprimir_diccionarios_cargados()
{
    printf("\n=== RADIOGRAFÍA DE LOS DICCIONARIOS EN MEMORIA ===\n");
    for (int i = 0; i < MAX_DICCIONARIOS; i++)
    {
        // Solo imprimimos si la clase tiene un nombre válido
        if (strlen(diccionarios[i].clase) > 0)
        {
            printf("-> Clase: '%s' (Contiene %d palabras clave)\n", diccionarios[i].clase, diccionarios[i].total_palabras);
            printf("   Palabras en memoria: ");

            // Recorremos y mostramos cada palabra exacta que se guardó
            for (int j = 0; j < diccionarios[i].total_palabras; j++)
            {
                printf("[%s] ", diccionarios[i].palabras[j]);
            }
            printf("\n");
        }
    }
    printf("==================================================\n\n");
}

// cargar diccionarios dinámicamente ---
void cargar_diccionarios()
{
    FILE *archivo = fopen("diccionarios.txt", "r");
    if (!archivo)
    {
        perror("[-] Error: No se encontró diccionarios.txt");
        exit(EXIT_FAILURE);
    }

    char linea[512];
    int i = 0;
    while (fgets(linea, sizeof(linea), archivo) && i < MAX_DICCIONARIOS)
    {
        linea[strcspn(linea, "\n")] = 0; // Limpiar salto de línea (\n)

        char *clase = strtok(linea, ":");
        if (!clase)
            continue; // Protección extra
        strcpy(diccionarios[i].clase, clase);

        char *palabras_str = strtok(NULL, ":");
        if (!palabras_str)
            continue;

        // Agregamos un espacio en blanco ' ' y un retorno de carro '\r' a los delimitadores.
        // Esto purifica la palabra y elimina cualquier basura invisible del .txt
        char *palabra = strtok(palabras_str, ", \r\t");

        int j = 0;
        while (palabra != NULL && j < MAX_PALABRAS)
        {
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

void determinar_tipo_usuario()
{
    pthread_mutex_lock(&stats.lock);
    int total = stats.total_documentos;

    if (total == 0)
    {
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

    // Lógica de tabla de referencia
    if (prop_correo > 0.5 && prop_reporte > 0.3)
    {
        printf("[!] Usuario detectado: Personal administrativo\n");
    }
    else if (prop_correo > 0.4 && prop_reporte > 0.4)
    {
        printf("[!] Usuario detectado: Personal técnico\n");
    }
    else if (prop_correo > 0.3 && prop_cient > 0.4)
    {
        printf("[!] Usuario detectado: Profesor\n");
    }
    else
    {
        printf("[!] Usuario detectado: Estudiante\n");
    }
    printf("=========================================\n");

    pthread_mutex_unlock(&stats.lock);
}

// SALIR DEL SERVIDOR
void handle_sigint(int sig)
{
    printf("\n[IALearner] Apagando servidor... ejecutando inferencia final.\n");
    determinar_tipo_usuario(); // <--- AQUÍ EJECUTAS LA INFERENCIA
    exit(0);
}

// Función que ejecutará cada hilo para escuchar a una ventana específica
void *atender_ventana(void *socket_desc)
{
    int sock = *(int *)socket_desc;
    char buffer[1024] = {0};

    char oracion[2048] = {0};
    char documento_completo[8192] = {0}; // Almacena Todo lo que el usuario escribió
    char id_proceso[20] = {0};
    int valread;

    // --- 1. FASE DE RECOLECCIÓN ---
    while ((valread = read(sock, buffer, 1024)) > 0)
    {
        char tecla[50] = {0};

        // Extraemos quién envía y qué tecla es (Ej: "P1: A")
        if (sscanf(buffer, "P%[^:]: %s", id_proceso, tecla) == 2)
        {

            if (strcmp(tecla, "Return") == 0)
            {
                if (strlen(oracion) > 0)
                {
                    printf("\n[Data Center] [P%s] Oración completada: %s\n", id_proceso, oracion);
                    pthread_mutex_lock(&mutex_queue);

                    if (queue_count < MAX_QUEUE_SIZE)
                    {
                        strcpy(sentence_queue[queue_count].sentence, oracion);
                        sentence_queue[queue_count].client_id = atoi(id_proceso);
                        queue_count++;

                        printf("[Data Center] [P%s] Oración agregada a cola. Total en cola: %d/%d\n",
                               id_proceso, queue_count, P_param);

                        // Si ya juntamos P oraciones, despertamos al Loader
                        if (queue_count >= P_param)
                        {
                            pthread_cond_signal(&cond_loader);
                        }
                    }

                    pthread_mutex_unlock(&mutex_queue);
                    memset(oracion, 0, sizeof(oracion)); // Limpiar para la siguiente oración
                }
            }
            else if (strcmp(tecla, "space") == 0)
            {
                // Traducimos la tecla de espacio a un espacio real
                strcat(oracion, " ");
            }
            else if (strlen(tecla) == 1)
            {
                // Concatenamos las letras normales
                strcat(oracion, tecla);
            }
        }
        // Limpiamos el buffer de red
        memset(buffer, 0, sizeof(buffer));
    }

    close(sock);
    free(socket_desc);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("[-] Error: Debe ingresar el parámetro P.\n");
        printf("Uso correcto: %s <P>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    P_param = atoi(argv[1]);
    printf("[IALearner] Parámetro P configurado a: %d\n", P_param);

    cargar_diccionarios();
    imprimir_diccionarios_cargados();

    // --- NUEVO: Arrancar el hilo Loader y el pool de P hilos detectores ---
    pthread_t loader_thread;
    pthread_create(&loader_thread, NULL, hilo_loader, NULL);
    pthread_detach(loader_thread);

    for (int i = 0; i < P_param; i++)
    {
        pthread_t detector_thread;
        pthread_create(&detector_thread, NULL, hilo_detector, NULL);
        pthread_detach(detector_thread);
    }
    printf("[IALearner] Pool de %d hilos detectores e hilo Loader iniciados.\n", P_param);
    // normal anterior proyecto
    FILE *archivo = fopen("config.txt", "r");
    if (archivo == NULL)
    {
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
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket falló");
        exit(EXIT_FAILURE);
    }

    // ---> NUEVA DEFENSA: RECICLAJE INMEDIATO DEL PUERTO <---
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
    {
        perror("Error al configurar SO_REUSEADDR");
        exit(EXIT_FAILURE);
    } // termina

    signal(SIGINT, handle_sigint);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Escucha en cualquier IP local
    address.sin_port = htons(puerto_config);

    // 2. Vincular el socket al puerto 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind falló");
        exit(EXIT_FAILURE);
    }

    // 3. Escuchar conexiones (hasta 10 ventanas en cola)
    if (listen(server_fd, 10) < 0)
    {
        perror("Listen falló");
        exit(EXIT_FAILURE);
    }

    printf("=========================================\n");
    printf("[IALearner] Data Center iniciado.\n");
    printf("[IALearner] Escuchando en el puerto %d...\n", puerto_config);
    printf("=========================================\n");

    // 4. Bucle infinito para aceptar ventanas (clientes)
    while (1)
    {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("Accept falló");
            continue;
        }

        printf("\n[IALearner] [+] ¡Nueva ventana X11 conectada!\n");

        // Crear un hilo (Pthread) para que atienda a esta ventana concurrentemente
        pthread_t thread_id;
        int *new_sock = malloc(sizeof(int));
        *new_sock = new_socket;

        if (pthread_create(&thread_id, NULL, atender_ventana, (void *)new_sock) < 0)
        {
            perror("No se pudo crear el hilo");
            free(new_sock);
        }

        // Separar el hilo para que libere su memoria al terminar sin dejar zombies
        pthread_detach(thread_id);
    }

    return 0;
}
