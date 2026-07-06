Agentic OS: Data Intelligence Center
Sistema distribuido de monitoreo de actividad, compuesto por un motor de inferencia concurrente (IALearner), agentes de captura (x11_keys) y un orquestador (launcher).


Estructura del Proyecto
IALearner.c: Servidor central. Clasifica usuarios mediante el método "Bag of Words".
x11_keys.c: Cliente de captura. Monitorea teclas y envía eventos vía TCP.
launcher.c: Orquestador. Gestiona el ciclo de vida de los procesos hijos mediante fork/exec.


Requisitos Previos
Compilador GCC y librerías X11: sudo apt-get install libx11-dev.


Compilación y Ejecución
Ejecuta make para compilar todo.
Inicia el servidor: ./IALearner
En otra terminal, abre el orquestador: ./launcher