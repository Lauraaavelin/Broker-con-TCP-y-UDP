// servidor
// socket -> bind -> listen -> accept
// recv -> send
// el broker acepta conexiones TCP persistentes, distingue roles y reenvia publicaciones a los suscriptores correctos

#include <stdio.h> // printf, perror
#include <stdlib.h> // exit
#include <unistd.h> // close
#include <string.h> // strcmp, strncmp, strncpy, strlen, strcspn, memset, sscanf
#include <sys/socket.h> // socket, bind, listen, accept, recv, send, setsockopt
#include <sys/select.h> // select, fd_set, FD_ZERO, FD_SET, FD_ISSET
#include <netinet/in.h> // sockaddr_in, htons, htonl, INADDR_ANY, socklen_t
#include <arpa/inet.h> // inet_ntop, INET_ADDRSTRLEN
#include <signal.h> // signal, SIGINT
#include <errno.h> // errno, EINTR

#define PUERTO 5222 // puerto fijo del broker
#define BACKLOG 5 // máximo de conexiones pendientes en cola para hacer accept
#define MAX_CLIENTES 10 // máximo de clientes simultáneos
#define TAM_BUFFER 1024 // tamaño del buffer de lectura
#define MAX_TOPICOS 10 // máximo de tópicos por cliente
#define MAX_NOMBRE_TOPICO 64 // tamaño máximo del nombre del tópico

#define ROL_NINGUNO 0 // cliente conectado pero aún sin rol
#define ROL_PUBLISHER 1 // cliente publicador
#define ROL_SUBSCRIBER 2 // cliente suscriptor

// Asi es la representacion de cada cliente conectado al broker, con su socket, estado, rol, lista de topicos y datos de conexion
typedef struct {
    int socket_cliente; // descriptor del socket de este cliente
    int activo; // 1 si esta posición del arreglo está ocupada, 0 si está libre
    int rol; // rol del cliente: publisher, subscriber o ninguno
    char ip[INET_ADDRSTRLEN]; // IP del cliente en formato texto
    int puerto; // puerto remoto del cliente
    char topicos[MAX_TOPICOS][MAX_NOMBRE_TOPICO]; // lista de tópicos del cliente según su rol
    int cantidad_topicos; // cantidad de tópicos actualmente guardados
} Cliente;

static int socket_broker = -1; // socket principal de escucha del broker
static Cliente clientes[MAX_CLIENTES]; // arreglo global de clientes conectados

const char *TOPICOS_VALIDOS[3] = {
    "Santa_Fe_vs_Millonarios",
    "Uniandes_vs_Javeriana",
    "Colombia_vs_Francia"
}; // topicos fijos definidos por el broker

void cerrar_todo_y_salir(int codigo);
void manejar_ctrl_c(int senal);
void inicializar_clientes(void);
int agregar_cliente(int socket_nuevo, struct sockaddr_in *direccion_cliente);
void eliminar_cliente(int indice);
void enviar_texto(int socket_destino, const char *texto);
int topico_es_valido(const char *topico);
int cliente_tiene_topico(int indice_cliente, const char *topico);
void agregar_topico_a_cliente(int indice_cliente, const char *topico);
void registrar_rol_publisher(int indice_cliente);
void registrar_rol_subscriber(int indice_cliente);
void registrar_suscripcion(int indice, const char *topico);
void publicar_mensaje(int indice_publicador, const char *topico, const char *mensaje);
void procesar_mensaje_cliente(int indice, char *buffer);

int main(void) {
    int resultado_bind; // guardara 0 si bind sale bien y -1 si falla
    int resultado_listen; // guardara 0 si listen sale bien y -1 si falla
    int opcion_reusar = 1; // permite reutilizar el puerto rápidamente al reiniciar
    int maximo_socket; // mayor descriptor entre el socket del broker y los clientes activos
    int actividad; // resultado de select para saber si hubo actividad en algun socket
    int socket_nuevo; // socket que devuelve accept al aceptar una nueva conexión
    int indice_nuevo; // posición del arreglo donde se guardó el nuevo cliente
    int i; // índice para recorrer clientes
    ssize_t bytes_recibidos; // cantidad de bytes que devuelve recv
    char buffer[TAM_BUFFER]; // buffer temporal para recibir mensajes
    fd_set conjunto_lectura; // conjunto de sockets que select debe vigilar
    struct sockaddr_in direccion_broker = {0}; // direccion local del broker
    struct sockaddr_in direccion_cliente = {0}; // direccion del cliente que se conecta
    struct sockaddr *direccion_generica_broker; // misma direccion del broker, pero vista como sockaddr generica
    struct sockaddr *direccion_generica_cliente; // misma direccion del cliente, pero vista como sockaddr generica
    socklen_t tam_direccion_cliente; // tamaño de la estructura direccion_cliente para accept

    signal(SIGINT, manejar_ctrl_c); // registra la funcion que se ejecuta si el usuario presiona ctrl+c
    inicializar_clientes(); // deja todo el arreglo de clientes en estado limpio e inactivo

    socket_broker = socket(AF_INET, SOCK_STREAM, 0); // crea un socket TCP sobre IPv4 para el broker
    if (socket_broker < 0) { perror("socket"); exit(1); } // si falla, muestra error y termina

    if (setsockopt(socket_broker, SOL_SOCKET, SO_REUSEADDR, &opcion_reusar, sizeof(opcion_reusar)) < 0) { perror("setsockopt"); cerrar_todo_y_salir(1); } // permite reutilizar el puerto al reiniciar sin esperar demasiado

    direccion_broker.sin_family = AF_INET; // indica que esta direccion es IPv4
    direccion_broker.sin_addr.s_addr = htonl(INADDR_ANY); // escucha por cualquier IP local de esta maquina
    direccion_broker.sin_port = htons(PUERTO); // guarda el puerto del broker en formato de red

    direccion_generica_broker = (struct sockaddr *)&direccion_broker; // mismo bloque de memoria, visto como direccion generica

    resultado_bind = bind(socket_broker, direccion_generica_broker, sizeof(direccion_broker)); // asocia el socket del broker con su IP local y su puerto local
    if (resultado_bind < 0) { perror("bind"); cerrar_todo_y_salir(1); } // si falla, cierra todo y termina

    resultado_listen = listen(socket_broker, BACKLOG); // activa la escucha y deja una cola de conexiones pendientes
    if (resultado_listen < 0) { perror("listen"); cerrar_todo_y_salir(1); } // si falla, cierra todo y termina

    printf("Broker escuchando en el puerto %d\n", PUERTO); // confirma que el broker quedo listo
    printf("Topicos validos:\n"); // muestra los topicos permitidos por el broker
    printf("  %s\n", TOPICOS_VALIDOS[0]); // muestra topico 1
    printf("  %s\n", TOPICOS_VALIDOS[1]); // muestra topico 2
    printf("  %s\n", TOPICOS_VALIDOS[2]); // muestra topico 3
    printf("Comandos esperados:\n"); // muestra la guia basica del protocolo de aplicacion
    printf("  ROLE PUBLISHER\n"); // comando para declararse publisher
    printf("  ROLE SUBSCRIBER\n"); // comando para declararse subscriber
    printf("  SUB <topico>\n"); // comando para suscribirse a un topico
    printf("  PUB <topico> <mensaje>\n"); // comando para publicar un mensaje
    printf("  EXIT\n"); // comando para salir
    printf("Ctrl+C para cerrar el broker\n"); // recuerda que ctrl+c cierra ordenadamente el programa

    while (1) { // ciclo principal del broker; se repite indefinidamente mientras el broker siga vivo

        FD_ZERO(&conjunto_lectura); // limpia el conjunto de sockets que select va a vigilar
        FD_SET(socket_broker, &conjunto_lectura); // agrega el socket de escucha del broker al conjunto
        maximo_socket = socket_broker; // al principio, el mayor descriptor es el del broker

        for (i = 0; i < MAX_CLIENTES; i++) { // recorre todos los clientes del arreglo
            if (clientes[i].activo) { // solo tiene en cuenta los clientes que sí están conectados
                FD_SET(clientes[i].socket_cliente, &conjunto_lectura); // agrega el socket de ese cliente al conjunto vigilado
                if (clientes[i].socket_cliente > maximo_socket) { maximo_socket = clientes[i].socket_cliente; } // actualiza el valor del mayor descriptor si hace falta
            }
        }

        actividad = select(maximo_socket + 1, &conjunto_lectura, NULL, NULL, NULL); // se bloquea hasta que algún socket del conjunto tenga actividad lista
        if (actividad < 0) { // entra si select devolvio error
            if (errno == EINTR) { continue; } // si la interrupcion fue por una señal como ctrl+c, vuelve al inicio del ciclo
            perror("select"); // muestra error real de select
            break; // sale del while para cerrar el broker de forma ordenada
        }

        if (FD_ISSET(socket_broker, &conjunto_lectura)) { // revisa si el socket del broker tiene una nueva conexión pendiente

            tam_direccion_cliente = sizeof(direccion_cliente); // le dice a accept cuánto espacio hay disponible para escribir la direccion del cliente
            direccion_generica_cliente = (struct sockaddr *)&direccion_cliente; // mismo bloque de memoria, visto como direccion generica

            socket_nuevo = accept(socket_broker, direccion_generica_cliente, &tam_direccion_cliente); // acepta la nueva conexión y devuelve un socket nuevo para ese cliente

            if (socket_nuevo < 0) { // entra si accept falla
                perror("accept"); // muestra error real de accept
            } else { // entra si accept aceptó correctamente al cliente
                indice_nuevo = agregar_cliente(socket_nuevo, &direccion_cliente); // intenta guardar el nuevo cliente en el arreglo global

                if (indice_nuevo < 0) { // entra si no había espacio libre en el arreglo
                    enviar_texto(socket_nuevo, "ERR broker lleno\n"); // avisa al cliente que el broker ya no tiene cupo
                    close(socket_nuevo); // cierra el socket recién aceptado porque no se puede manejar
                } else { // entra si sí se pudo guardar el cliente
                    printf("cliente conectado: socket=%d %s:%d\n", clientes[indice_nuevo].socket_cliente, clientes[indice_nuevo].ip, clientes[indice_nuevo].puerto); // imprime datos del nuevo cliente
                    enviar_texto(socket_nuevo, "OK conectado al broker tcp\n"); // responde confirmando que quedó conectado al broker
                }
            }
        }

        for (i = 0; i < MAX_CLIENTES; i++) { // recorre todos los clientes para revisar si alguno envió datos

            if (!clientes[i].activo) { continue; } // si esa posicion no tiene cliente activo, la ignora
            if (!FD_ISSET(clientes[i].socket_cliente, &conjunto_lectura)) { continue; } // si ese cliente no fue marcado por select como listo para leer, lo ignora

            memset(buffer, 0, sizeof(buffer)); // limpia el buffer antes de volver a usarlo
            bytes_recibidos = recv(clientes[i].socket_cliente, buffer, sizeof(buffer) - 1, 0); // intenta leer datos desde el socket del cliente

            if (bytes_recibidos < 0) { // entra si recv devolvio error
                perror("recv"); // muestra el error real de recv
                eliminar_cliente(i); // elimina al cliente del arreglo y cierra su socket
                continue; // sigue con el siguiente cliente del arreglo
            }

            if (bytes_recibidos == 0) { // entra si recv devolvio 0, que significa que el cliente cerró la conexión
                eliminar_cliente(i); // elimina al cliente del arreglo y cierra su socket
                continue; // sigue con el siguiente cliente del arreglo
            }

            buffer[bytes_recibidos] = '\0'; // agrega fin de string al final de los bytes recibidos para poder tratar el buffer como texto

            printf("socket=%d envio: %s", clientes[i].socket_cliente, buffer); // imprime en consola lo que mandó el cliente
            if (buffer[strlen(buffer) - 1] != '\n') { printf("\n"); } // si el mensaje no terminaba en salto de linea, imprime uno para mantener ordenada la consola

            procesar_mensaje_cliente(i, buffer); // interpreta el mensaje recibido y ejecuta la logica correspondiente del broker
        }
    }

    cerrar_todo_y_salir(0); // si se rompe el while principal, cierra todo ordenadamente y termina
}

void cerrar_todo_y_salir(int codigo) {
    /*
    Aqui se recorren todos los clientes activos y se cierran sus sockets, luego se cierra el socket del servidor y se sale del programa con el codigo indicado.
    */
    int i; // indice para recorrer el arreglo de clientes

    if (socket_broker != -1) { // entra si el socket principal del broker sí existe
        close(socket_broker); // se cierra el socket del servidor para liberar el puerto y recursos asociados
    }

    for (i = 0; i < MAX_CLIENTES; i++) { // se cierra cliente por cliente los que siguen activos para liberar recursos y evitar conexiones colgadas
        if (clientes[i].activo) { // entra solo si esa posicion del arreglo tiene un cliente activo
            close(clientes[i].socket_cliente); // cierra el socket de ese cliente
            clientes[i].activo = 0; // marca la posicion como inactiva
        }
    }

    exit(codigo); // termina el programa con el codigo de salida recibido
}

void manejar_ctrl_c(int senal) {
    // Esta funcion se ejecuta cuando el usuario presiona ctrl+c, se encarga de cerrar todo y salir del programa de forma ordenada.
    (void)senal; // evita advertencia por parametro no usado
    printf("\ncerrando broker tcp...\n"); // informa en consola que el broker se cerrara
    cerrar_todo_y_salir(0); // cierra todo y termina correctamente
}

void inicializar_clientes(void) {
    /*
        Esta funcion se encarga de inicializar el arreglo de clientes, marcando todos como inactivos y con valores por defecto para sus campos.
        El socket_cliente se pone en -1 para indicar que no hay conexion, activo en 0 para indicar que no esta conectado, rol en ROL_NINGUNO para indicar que aun no tiene rol, topicos e ip se inicializan vacios y puerto en 0.
        Se crean todos los clientes esperados osea el maximo de clientes que se pueden tener, luego si cambiamos la info cuando un cliente real se conecte.
    */
    int i; // indice para recorrer el arreglo de clientes
    int j; // indice para recorrer la lista de topicos de cada cliente

    for (i = 0; i < MAX_CLIENTES; i++) { // recorre todas las posiciones del arreglo de clientes
        clientes[i].socket_cliente = -1; // marca que no hay socket valido en esa posicion
        clientes[i].activo = 0; // marca la posicion como libre
        clientes[i].rol = ROL_NINGUNO; // deja el rol como no definido
        clientes[i].ip[0] = '\0'; // deja la IP como string vacio
        clientes[i].puerto = 0; // reinicia el puerto
        clientes[i].cantidad_topicos = 0; // reinicia la cantidad de topicos guardados

        for (j = 0; j < MAX_TOPICOS; j++) { // recorre todas las posiciones del arreglo de topicos de ese cliente
            clientes[i].topicos[j][0] = '\0'; // deja cada topico como string vacio
        }
    }
}

int agregar_cliente(int socket_nuevo, struct sockaddr_in *direccion_cliente) {
    /*
    Esta funcion se encarga de agregar un nuevo cliente al arreglo de clientes, buscando una posicion libre y llenando los datos del cliente con la informacion del socket y la direccion del cliente.
    */
    int i; // indice para recorrer el arreglo de clientes

    for (i = 0; i < MAX_CLIENTES; i++) { // recorre todas las posiciones del arreglo de clientes
        if (!clientes[i].activo) { // encuentra una posicion que este libre
            clientes[i].socket_cliente = socket_nuevo; // guarda el socket del nuevo cliente
            clientes[i].activo = 1; // marca esta posicion como ocupada
            clientes[i].rol = ROL_NINGUNO; // al comienzo el cliente aun no ha definido su rol
            clientes[i].cantidad_topicos = 0; // empieza sin topicos guardados

            inet_ntop(AF_INET, &(direccion_cliente->sin_addr), clientes[i].ip, sizeof(clientes[i].ip)); // convierte la IP del cliente a formato legible y la guarda en texto
            clientes[i].puerto = ntohs(direccion_cliente->sin_port); // guarda el puerto del cliente en formato host

            return i; // devuelve la posicion donde quedo guardado el nuevo cliente
        }
    }

    return -1; // devuelve -1 si no habia ninguna posicion libre
}

void eliminar_cliente(int indice) {
    /*
    Esta funcion se encarga de eliminar un cliente del arreglo de clientes, cerrando su socket y marcando su posicion como inactiva.
    */
    int j; // indice para limpiar la lista de topicos del cliente

    if (indice < 0 || indice >= MAX_CLIENTES || !clientes[indice].activo) { // verifica que el indice sea valido y que el cliente esté activo antes de intentar eliminarlo
        return; // si no se cumple, sale sin hacer nada
    }

    printf("cliente desconectado: socket=%d %s:%d\n", clientes[indice].socket_cliente, clientes[indice].ip, clientes[indice].puerto); // informa cuál cliente se desconectó

    close(clientes[indice].socket_cliente); // se cierra el socket del cliente para liberar recursos
    clientes[indice].socket_cliente = -1; // deja el campo socket en valor invalido
    clientes[indice].activo = 0; // marca la posicion como libre
    clientes[indice].rol = ROL_NINGUNO; // reinicia el rol
    clientes[indice].ip[0] = '\0'; // reinicia la IP
    clientes[indice].puerto = 0; // reinicia el puerto
    clientes[indice].cantidad_topicos = 0; // reinicia la cantidad de topicos

    for (j = 0; j < MAX_TOPICOS; j++) { // recorre toda la lista de topicos del cliente
        clientes[indice].topicos[j][0] = '\0'; // vacia cada string de topico
    }
}

void enviar_texto(int socket_destino, const char *texto) {
    /*
        Esta función se encarga de enviar un texto a un socket destino, utilizando la funcion send y manejando posibles errores.
        La funcion send devuelve el numero de bytes enviados o -1 en caso de error, por lo que se verifica si el resultado es negativo para imprimir un mensaje de error con perror.
    */
    if (send(socket_destino, texto, strlen(texto), 0) < 0) { // intenta enviar el texto por el socket indicado
        perror("send"); // muestra el error real si el envio falla
    }
}

int topico_es_valido(const char *topico) {
    /*
        Esta funcion revisa si el topico recibido esta dentro de la lista fija de topicos que el broker permite manejar.
        Devuelve 1 si el topico existe y 0 si no existe.
    */
    int i; // indice para recorrer la lista de topicos validos

    for (i = 0; i < 3; i++) { // recorre los tres topicos definidos en el broker
        if (strcmp(topico, TOPICOS_VALIDOS[i]) == 0) { // compara el topico recibido con cada topico valido
            return 1; // devuelve 1 si encontro coincidencia
        }
    }

    return 0; // devuelve 0 si no encontro el topico en la lista valida
}

int cliente_tiene_topico(int indice_cliente, const char *topico) {
    /*
        Esta funcion revisa si el cliente indicado ya tiene guardado ese topico en su lista.
        Sirve tanto para no repetir suscripciones como para registrar en que topicos ya ha publicado un publisher.
    */
    int i; // indice para recorrer la lista de topicos del cliente

    for (i = 0; i < clientes[indice_cliente].cantidad_topicos; i++) { // recorre todos los topicos actualmente guardados por ese cliente
        if (strcmp(clientes[indice_cliente].topicos[i], topico) == 0) { // compara el topico buscado con cada topico guardado
            return 1; // devuelve 1 si sí lo encontró
        }
    }

    return 0; // devuelve 0 si no encontró el topico
}

void agregar_topico_a_cliente(int indice_cliente, const char *topico) {
    /*
        Esta funcion agrega un topico nuevo a la lista del cliente, siempre que aun haya espacio en el arreglo de topicos.
        La lista significa cosas distintas segun el rol:
        - si es subscriber, son topicos a los que esta suscrito
        - si es publisher, son topicos en los que ya ha publicado
    */
    if (clientes[indice_cliente].cantidad_topicos < MAX_TOPICOS) { // entra solo si aun hay espacio en el arreglo de topicos
        strncpy(clientes[indice_cliente].topicos[clientes[indice_cliente].cantidad_topicos], topico, MAX_NOMBRE_TOPICO - 1); // copia el nombre del topico a la siguiente posicion libre
        clientes[indice_cliente].topicos[clientes[indice_cliente].cantidad_topicos][MAX_NOMBRE_TOPICO - 1] = '\0'; // asegura fin de string en la ultima posicion
        clientes[indice_cliente].cantidad_topicos++; // incrementa la cantidad de topicos guardados
    }
}

void registrar_rol_publisher(int indice_cliente) {
    /*
        Esta funcion marca al cliente indicado como publisher y le responde confirmando el cambio de rol.
    */
    clientes[indice_cliente].rol = ROL_PUBLISHER; // guarda el rol publisher en la estructura del cliente
    enviar_texto(clientes[indice_cliente].socket_cliente, "OK ROLE PUBLISHER\n"); // confirma al cliente que su rol quedó registrado
    printf("cliente socket=%d registrado como publisher\n", clientes[indice_cliente].socket_cliente); // informa en consola el cambio de rol
}

void registrar_rol_subscriber(int indice_cliente) {
    /*
        Esta funcion marca al cliente indicado como subscriber y le responde confirmando el cambio de rol.
    */
    clientes[indice_cliente].rol = ROL_SUBSCRIBER; // guarda el rol subscriber en la estructura del cliente
    enviar_texto(clientes[indice_cliente].socket_cliente, "OK ROLE SUBSCRIBER\n"); // confirma al cliente que su rol quedó registrado
    printf("cliente socket=%d registrado como subscriber\n", clientes[indice_cliente].socket_cliente); // informa en consola el cambio de rol
}

void registrar_suscripcion(int indice, const char *topico) {
    /*
        Esta función se encarga de registrar una suscripción para un cliente subscriber,
        validando que el topico exista y que aún no estuviera ya guardado.
        Luego se envía una respuesta de confirmación al cliente con el formato "ACK SUB <tema>\n" para indicar que la suscripción fue exitosa.
    */
    char respuesta[TAM_BUFFER]; // buffer temporal para construir la respuesta

    if (clientes[indice].rol != ROL_SUBSCRIBER) { // entra si el cliente aun no se ha declarado subscriber
        enviar_texto(clientes[indice].socket_cliente, "ERR primero haga ROLE SUBSCRIBER\n"); // responde que primero debe definir el rol correcto
        return; // sale sin registrar la suscripcion
    }

    if (!topico_es_valido(topico)) { // entra si el topico recibido no esta dentro de los topicos permitidos
        enviar_texto(clientes[indice].socket_cliente, "ERR topico no valido\n"); // responde que el topico no existe
        return; // sale sin registrar la suscripcion
    }

    if (cliente_tiene_topico(indice, topico)) { // entra si el cliente ya estaba suscrito a ese topico
        enviar_texto(clientes[indice].socket_cliente, "ERR ya estaba suscrito a ese topico\n"); // evita duplicar la suscripcion
        return; // sale sin volver a agregarlo
    }

    agregar_topico_a_cliente(indice, topico); // agrega el topico a la lista del cliente
    snprintf(respuesta, sizeof(respuesta), "ACK SUB %s\n", topico); // construye la respuesta de confirmacion
    enviar_texto(clientes[indice].socket_cliente, respuesta); // envia la respuesta al cliente

    printf("suscriptor registrado: socket=%d topico=%s\n", clientes[indice].socket_cliente, topico); // imprime en consola la nueva suscripcion
}

void publicar_mensaje(int indice_publicador, const char *topico, const char *mensaje) {
    /*
        Esta funcion obtiene el mensaje publicado por un cliente publisher, lo formatea con el prefijo "MSG <topico> <mensaje>\n"
        y lo envia a todos los clientes que sean subscribers y esten suscritos al mismo topico.
        Luego cuenta cuantas entregas se hicieron y envia una respuesta al publicador con el formato "ACK PUB <topico> <entregados>\n".
    */
    int i; // indice para recorrer todos los clientes
    int j; // indice para recorrer la lista de topicos del cliente actual
    int entregados = 0; // contador de cuantos subscribers recibieron el mensaje
    char mensaje_salida[TAM_BUFFER]; // buffer para construir el mensaje que se reenviara
    char respuesta[TAM_BUFFER]; // buffer para construir la respuesta al publisher

    if (clientes[indice_publicador].rol != ROL_PUBLISHER) { // entra si el cliente aun no se ha declarado publisher
        enviar_texto(clientes[indice_publicador].socket_cliente, "ERR primero haga ROLE PUBLISHER\n"); // responde que primero debe definir el rol correcto
        return; // sale sin publicar
    }

    if (!topico_es_valido(topico)) { // entra si el topico no esta entre los permitidos
        enviar_texto(clientes[indice_publicador].socket_cliente, "ERR topico no valido\n"); // responde con error
        return; // sale sin publicar
    }

    if (!cliente_tiene_topico(indice_publicador, topico)) { // revisa si el publisher ya tenia registrado este topico
        agregar_topico_a_cliente(indice_publicador, topico); // si no lo tenia, lo agrega a su lista de topicos usados
    }

    snprintf(mensaje_salida, sizeof(mensaje_salida), "MSG %s %s\n", topico, mensaje); // formatea el mensaje que veran los subscribers

    for (i = 0; i < MAX_CLIENTES; i++) { // recorre todos los clientes del broker
        if (!clientes[i].activo) { continue; } // si la posicion no tiene cliente activo, la ignora
        if (clientes[i].rol != ROL_SUBSCRIBER) { continue; } // si el cliente no es subscriber, la ignora

        for (j = 0; j < clientes[i].cantidad_topicos; j++) { // recorre todos los topicos a los que ese subscriber esta suscrito
            if (strcmp(clientes[i].topicos[j], topico) == 0) { // compara cada topico del subscriber con el topico publicado
                if (send(clientes[i].socket_cliente, mensaje_salida, strlen(mensaje_salida), 0) < 0) { // intenta enviar el mensaje al subscriber
                    perror("send"); // muestra error si el envio falla
                } else { // entra si el envio salio bien
                    entregados++; // incrementa la cantidad de entregas realizadas
                }
                break; // sale del for interno porque ya encontro el topico en ese subscriber
            }
        }
    }

    snprintf(respuesta, sizeof(respuesta), "ACK PUB %s %d\n", topico, entregados); // construye la respuesta de confirmacion al publisher
    enviar_texto(clientes[indice_publicador].socket_cliente, respuesta); // envia la confirmacion al publisher

    printf("publicacion recibida: topico=%s entregados=%d mensaje=%s\n", topico, entregados, mensaje); // informa en consola la publicacion procesada
}

void procesar_mensaje_cliente(int indice, char *buffer) {
    /*
        Esta funcion se encarga de procesar un mensaje recibido de un cliente,
        identificando si es un comando de declaracion de rol, suscripcion, publicacion o salida.
        Si el formato del comando no es correcto, se envia una respuesta de error al cliente indicando el formato esperado.
        Si el comando no es reconocido, se envia una respuesta de error indicando que el comando no es reconocido.
    */
    char comando[16]; // guardara la primera palabra del mensaje
    char topico[MAX_NOMBRE_TOPICO]; // guardara el topico del mensaje
    char mensaje[TAM_BUFFER]; // guardara el contenido de una publicacion

    comando[0] = '\0'; // inicia el string comando vacio
    topico[0] = '\0'; // inicia el string topico vacio
    mensaje[0] = '\0'; // inicia el string mensaje vacio

    buffer[strcspn(buffer, "\r\n")] = '\0'; // elimina salto de linea final si lo trae

    if (strcmp(buffer, "ROLE PUBLISHER") == 0) { // revisa si el cliente quiere declararse publisher
        registrar_rol_publisher(indice); // llama a la funcion que registra el rol publisher
        return; // termina de procesar este mensaje
    }

    if (strcmp(buffer, "ROLE SUBSCRIBER") == 0) { // revisa si el cliente quiere declararse subscriber
        registrar_rol_subscriber(indice); // llama a la funcion que registra el rol subscriber
        return; // termina de procesar este mensaje
    }

    if (strncmp(buffer, "SUB ", 4) == 0) { // revisa si el mensaje empieza con SUB
        if (sscanf(buffer, "%15s %63s", comando, topico) == 2) { // intenta extraer comando y topico
            registrar_suscripcion(indice, topico); // registra la suscripcion del cliente al topico indicado
        } else { // entra si el formato no coincidio con dos campos
            enviar_texto(clientes[indice].socket_cliente, "ERR formato esperado: SUB <topico>\n"); // responde indicando el formato correcto
        }
        return; // termina de procesar este mensaje
    }

    if (strncmp(buffer, "PUB ", 4) == 0) { // revisa si el mensaje empieza con PUB
        if (sscanf(buffer, "%15s %63s %1023[^\n]", comando, topico, mensaje) == 3) { // intenta extraer comando, topico y contenido completo
            publicar_mensaje(indice, topico, mensaje); // publica el mensaje en el topico indicado
        } else { // entra si el formato no fue correcto
            enviar_texto(clientes[indice].socket_cliente, "ERR formato esperado: PUB <topico> <mensaje>\n"); // responde indicando el formato correcto
        }
        return; // termina de procesar este mensaje
    }

    if (strcmp(buffer, "EXIT") == 0) { // revisa si el cliente quiere salir de forma ordenada
        enviar_texto(clientes[indice].socket_cliente, "BYE\n"); // responde antes de cerrar
        eliminar_cliente(indice); // cierra el socket y elimina al cliente del arreglo
        return; // termina de procesar este mensaje
    }

    enviar_texto(clientes[indice].socket_cliente, "ERR comando no reconocido\n"); // si no coincidió con nada conocido, responde con error generico
}

/*
sockaddr_in viene de <netinet/in.h> y representa una direccion IPv4 de socket.

struct in_addr {
    uint32_t s_addr; // IP en binario
};

struct sockaddr_in {
    sa_family_t sin_family; // familia, por ejemplo AF_INET
    in_port_t sin_port; // puerto en formato de red
    struct in_addr sin_addr; // IP
    unsigned char sin_zero[8]; // relleno interno
};

bind no recibe sockaddr_in*, sino sockaddr*.
sockaddr es la version generica de una direccion de socket, porque bind no solo recibe direcciones IPv4.
A bind le doy un puntero a memoria y un tamaño; esa parte de memoria contiene sockaddr_in y es la que dice IPv4.

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

Entonces armamos la direccion como sockaddr_in porque es comoda para IPv4,
y luego hacemos cast a sockaddr* porque asi esta definida la API de sockets.
Ese cast no cambia los datos; solo cambia el tipo con que se los pasamos a bind y a accept.

select no lee mensajes ni acepta clientes por si solo.
Solo le pregunta al sistema operativo que sockets tienen actividad lista en este momento.

En este programa se usa para dos cosas:
1. saber si socket_broker tiene una conexion pendiente para hacer accept
2. saber si alguno de los clientes ya conectados tiene datos listos para hacer recv

Cada conexion TCP corresponde a un solo cliente.
Un cliente puede suscribirse a varios topicos y publicar en varios topicos usando la misma conexion.
No se crea un cliente nuevo por cada topico.
*/