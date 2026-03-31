// servidor
// socket -> bind -> listen -> accept
// recv -> send (espera que cliente inicie la conexión y responde)

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define PUERTO_BROKER 5000        // puerto donde escucha el broker
#define TAMANO_BUFFER 1024        // buffer temporal para recibir mensajes
#define MAXIMO_CLIENTES 50        // limite de esta implementacion
#define LONGITUD_TEMA 64          // espacio maximo para guardar un tema
#define COLA_ESCUCHA 10           // conexiones pendientes en listen

typedef struct {
    int socket_cliente;
    int activo;
    int es_suscriptor;
    char tema[LONGITUD_TEMA];
    char ip[INET_ADDRSTRLEN];
    int puerto;
} Cliente;
// Asi es la representacion de cada cliente conectado al broker, con su socket, estado, si es suscriptor, tema de interes y datos de conexion
static int socket_servidor = -1;
static Cliente clientes[MAXIMO_CLIENTES];

void cerrar_todo_y_salir(int codigo) {
    /*
    Aqui se recorren todos los clientes activos y se cierran sus sockets, luego se cierra el socket del servidor y se sale del programa con el codigo indicado.
    */
    int i;

    if (socket_servidor != -1) {
        close(socket_servidor); // se cierra el socket del servidor para liberar el puerto y recursos asociados
    }

    for (i = 0; i < MAXIMO_CLIENTES; i++) { // se cierra cliente por cliente los que siguen activos para liberar recursos y evitar fugas de memoria o conexiones colgadas
        if (clientes[i].activo) {
            close(clientes[i].socket_cliente);
            clientes[i].activo = 0;
        }
    }

    exit(codigo);
}

void manejar_ctrl_c(int senal) { 
    //Esta funcion se ejecuta cuando el usuario presiona ctrl+c, se encarga de cerrar todo y salir del programa de forma ordenada.
    (void)senal;
    printf("\ncerrando broker tcp...\n");
    cerrar_todo_y_salir(0);
}

void inicializar_clientes(void) {
    /*
        Esta funcion se encarga de inicializar el arreglo de clientes, marcando todos como inactivos y con valores por defecto para sus campos. 
        El socket_cliente se pone en -1 para indicar que no hay conexion, activo en 0 para indicar que no esta conectado, es_suscriptor en 0 para indicar que no es suscriptor, tema e ip se inicializan con cadenas vacias y puerto en 0.    
        Se crean todos los clientes esperados osea el maximo de clientes que se pueden tener, luego si cambiamos la info cuando un cliente real se conecte.
    
    */
    int i;

    for (i = 0; i < MAXIMO_CLIENTES; i++) {
        clientes[i].socket_cliente = -1;
        clientes[i].activo = 0;
        clientes[i].es_suscriptor = 0;
        clientes[i].tema[0] = '\0';
        clientes[i].ip[0] = '\0';
        clientes[i].puerto = 0;
    }
}

int agregar_cliente(int socket_nuevo, struct sockaddr_in *direccion_cliente) {
    /*
    Esta funcion se encarga de agregar un nuevo cliente al arreglo de clientes, buscando una posicion libre (donde activo sea 0) y llenando los datos del cliente con la informacion del socket y la direccion del cliente.
    */
    int i;

    for (i = 0; i < MAXIMO_CLIENTES; i++) {
        //Encuentra alguno que no este activo, lo llena con la info del nuevo cliente y lo marca como activo para que el broker lo tenga en cuenta en las siguientes iteraciones del ciclo principal.
        if (!clientes[i].activo) {
            clientes[i].socket_cliente = socket_nuevo;
            clientes[i].activo = 1;
            clientes[i].es_suscriptor = 0;
            clientes[i].tema[0] = '\0';
            // aqui se convierte la direccion ip del cliente a formato legible y se guarda en el campo ip del cliente, tambien se guarda el puerto en formato host para facilitar su uso posteriormente.
            inet_ntop(
                AF_INET,
                &(direccion_cliente->sin_addr),
                clientes[i].ip,
                sizeof(clientes[i].ip)
            );

            clientes[i].puerto = ntohs(direccion_cliente->sin_port);
            return i;
        }
    }

    return -1;
}

void eliminar_cliente(int indice) {
    /*
    Esta funcion se encarga de eliminar un cliente del arreglo de clientes, cerrando su socket y marcando su posicion como inactiva.
    */
    if (indice < 0 || indice >= MAXIMO_CLIENTES || !clientes[indice].activo) {// verifica que el indice sea valido y que el cliente este activo antes de intentar eliminarlo, si no es asi simplemente se retorna sin hacer nada.
        return;
    }

    printf(
        "cliente desconectado: socket=%d %s:%d\n",
        clientes[indice].socket_cliente,
        clientes[indice].ip,
        clientes[indice].puerto
    );

    close(clientes[indice].socket_cliente); // se cierra el socket del cliente para liberar recursos y evitar conexiones colgadas
    // se reinician los datos del cliente para marcarlo como inactivo y listo para ser reutilizado por otro cliente que se conecte posteriormente
    clientes[indice].socket_cliente = -1;
    clientes[indice].activo = 0;
    clientes[indice].es_suscriptor = 0;
    clientes[indice].tema[0] = '\0';
    clientes[indice].ip[0] = '\0';
    clientes[indice].puerto = 0;
}

void enviar_texto(int socket_destino, const char *texto) {
    /*
        Esta función se encarga de enviar un texto a un socket destino, utilizando la funcion send y manejando posibles errores. El texto se envia con su longitud calculada con strlen, y se agrega un salto de linea al final para facilitar la lectura por parte del cliente.
        La funcion send devuelve el numero de bytes enviados o -1 en caso de error, por lo que se verifica si el resultado es negativo para imprimir un mensaje de error con perror.

    */
    if (send(socket_destino, texto, strlen(texto), 0) < 0) {
        perror("send");
    }
}

void registrar_suscripcion(int indice, const char *tema) {
    /*
        Esta función se encarga de registrar una suscripción para un cliente,
         marcando su campo es_suscriptor como 1 y guardando el tema de interés en su campo tema. 
         Luego se envía una respuesta de confirmación al cliente con el formato "ACK SUB <tema>\n" para indicar que la suscripción fue exitosa.
          Finalmente se imprime un mensaje en la consola del broker indicando que se ha registrado un nuevo suscriptor con su socket y tema.
        Tenniendo en cuenta el indice de dicho cliente ya existente pero que se va a poneer como subscriptor, y el tema que se le asigna como su interes para recibir mensajes publicados posteriormente.
    
    */
    char respuesta[TAMANO_BUFFER];

    clientes[indice].es_suscriptor = 1;

    strncpy(clientes[indice].tema, tema, LONGITUD_TEMA - 1);
    clientes[indice].tema[LONGITUD_TEMA - 1] = '\0';

    snprintf(respuesta, sizeof(respuesta), "ACK SUB %s\n", clientes[indice].tema);
    enviar_texto(clientes[indice].socket_cliente, respuesta);

    printf(
        "suscriptor registrado: socket=%d tema=%s\n",
        clientes[indice].socket_cliente,
        clientes[indice].tema
    );
}

void publicar_mensaje(int indice_publicador, const char *tema, const char *mensaje) {
    /*
    Esta funcion obtiene el mensaje publicado por un cliente, lo formatea con el prefijo "MSG <tema> <mensaje>\n"
     y lo envia a todos los clientes que esten suscritos al mismo tema. 
     Luego se cuenta cuantas entregas se hicieron y se envia una respuesta al publicador con el formato "ACK PUB <tema> <entregados>\n" 
     para indicar que la publicacion fue procesada y cuantos clientes recibieron el mensaje.
    */
    int i;
    int entregados = 0;
    char mensaje_salida[TAMANO_BUFFER];
    char respuesta[TAMANO_BUFFER];

    snprintf(mensaje_salida, sizeof(mensaje_salida), "MSG %s %s\n", tema, mensaje); // aqui se formatea el mensaje de salida con el prefijo "MSG", el tema y el mensaje recibido, para que los clientes suscritos puedan identificarlo correctamente al recibirlo.

    for (i = 0; i < MAXIMO_CLIENTES; i++) {
        if (!clientes[i].activo) {
            continue;
        }

        if (!clientes[i].es_suscriptor) {
            continue;
        }

        if (strcmp(clientes[i].tema, tema) != 0) {
            continue;
        }
        // en los if anteriores se miro que el cliente estuviera activo que fuera subscriptor y que su tema de interes sea el mismo.
        // si todo se cumple entonces se usa send para mandar el mensaje al cliente suscrito, y se verifica si hubo un error en el envio para imprimir un mensaje de error con perror, 
        //si no hubo error se incrementa el contador de entregados para luego informar al publicador.
        if (send(
                clientes[i].socket_cliente,
                mensaje_salida,
                strlen(mensaje_salida),
                0
            ) < 0) {
            perror("send");
            continue;
        }

        entregados++;
    }
    /* luego de enviar el mensaje a todos los suscriptores correspondientes,
     se formatea la respuesta para el publicador con el prefijo "ACK PUB",
      el tema y la cantidad de entregas realizadas, y se envia al publicador para confirmar que su mensaje fue procesado.*/
    snprintf(respuesta, sizeof(respuesta), "ACK PUB %s %d\n", tema, entregados);
    enviar_texto(clientes[indice_publicador].socket_cliente, respuesta);

    printf(
        "publicacion recibida: tema=%s entregados=%d mensaje=%s\n",
        tema,
        entregados,
        mensaje
    );
}

void procesar_mensaje_cliente(int indice, char *buffer) {
    /*
        Esta funcion se encarga de procesar un mensaje recibido de un cliente, identificando si es un comando de suscripcion (SUB) o publicacion (PUB) y extrayendo los parametros correspondientes.
        y pues llama a las funciones correspondientes para registrar la suscripcion o publicar el mensaje. 
        Si el formato del comando no es correcto, se envia una respuesta de error al cliente indicando el formato esperado. 
        Si el comando no es reconocido, se envia una respuesta de error indicando que el comando no es reconocido.
    */
    char comando[16];
    char tema[LONGITUD_TEMA];
    char mensaje[TAMANO_BUFFER];

    comando[0] = '\0';
    tema[0] = '\0';
    mensaje[0] = '\0';

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if (strncmp(buffer, "SUB ", 4) == 0) {
        if (sscanf(buffer, "%15s %63s", comando, tema) == 2) {
            registrar_suscripcion(indice, tema);
        } else {
            enviar_texto(
                clientes[indice].socket_cliente,
                "ERR formato esperado: SUB <tema>\n"
            );
        }
        return;
    }

    if (strncmp(buffer, "PUB ", 4) == 0) {
        if (sscanf(buffer, "%15s %63s %1023[^\n]", comando, tema, mensaje) == 3) {
            publicar_mensaje(indice, tema, mensaje);
        } else {
            enviar_texto(
                clientes[indice].socket_cliente,
                "ERR formato esperado: PUB <tema> <mensaje>\n"
            );
        }
        return;
    }

    enviar_texto(clientes[indice].socket_cliente, "ERR comando no reconocido\n");
}

//lo importante
int main(void) {
    struct sockaddr_in direccion_servidor;
    struct sockaddr_in direccion_cliente;
    socklen_t tam_direccion_cliente;
    fd_set conjunto_lectura;
    int maximo_socket;
    int actividad;
    int socket_nuevo;
    int indice_nuevo;
    int i;
    int opcion_reusar = 1;
    char buffer[TAMANO_BUFFER];

    signal(SIGINT, manejar_ctrl_c);
    inicializar_clientes();

    // crear socket tcp del broker
    socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_servidor < 0) {
        perror("socket");
        return 1;
    }

    // permitir reutilizar el puerto al reiniciar rapido
    if (setsockopt(
            socket_servidor,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opcion_reusar,
            sizeof(opcion_reusar)
        ) < 0) {
        perror("setsockopt");
        cerrar_todo_y_salir(1);
    }

    memset(&direccion_servidor, 0, sizeof(direccion_servidor));
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_port = htons(PUERTO_BROKER);
    direccion_servidor.sin_addr.s_addr = htonl(INADDR_ANY);

    // asociar el socket al puerto local del broker
    if (bind(
            socket_servidor,
            (struct sockaddr *)&direccion_servidor,
            sizeof(direccion_servidor)
        ) < 0) {
        perror("bind");
        cerrar_todo_y_salir(1);
    }

    // poner el socket en modo escucha
    if (listen(socket_servidor, COLA_ESCUCHA) < 0) {
        perror("listen");
        cerrar_todo_y_salir(1);
    }

    printf("broker tcp escuchando en el puerto %d\n", PUERTO_BROKER);
    printf("comandos esperados:\n");
    printf("  SUB <tema>\n");
    printf("  PUB <tema> <mensaje>\n");
    printf("ctrl+c para salir\n");

    while (1) {
        FD_ZERO(&conjunto_lectura);
        FD_SET(socket_servidor, &conjunto_lectura);
        maximo_socket = socket_servidor;

        // agregar clientes activos al conjunto de lectura
        for (i = 0; i < MAXIMO_CLIENTES; i++) {
            if (clientes[i].activo) {
                FD_SET(clientes[i].socket_cliente, &conjunto_lectura);

                if (clientes[i].socket_cliente > maximo_socket) {
                    maximo_socket = clientes[i].socket_cliente;
                }
            }
        }

        // esperar actividad en el broker o en clientes conectados
        actividad = select(maximo_socket + 1, &conjunto_lectura, NULL, NULL, NULL);
        if (actividad < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("select");
            break;
        }

        // nueva conexion
        if (FD_ISSET(socket_servidor, &conjunto_lectura)) {
            tam_direccion_cliente = sizeof(direccion_cliente);

            socket_nuevo = accept(
                socket_servidor,
                (struct sockaddr *)&direccion_cliente,
                &tam_direccion_cliente
            );

            if (socket_nuevo < 0) {
                perror("accept");
            } else {
                indice_nuevo = agregar_cliente(socket_nuevo, &direccion_cliente);

                if (indice_nuevo < 0) {
                    enviar_texto(socket_nuevo, "ERR broker lleno\n");
                    close(socket_nuevo);
                } else {
                    printf(
                        "cliente conectado: socket=%d %s:%d\n",
                        clientes[indice_nuevo].socket_cliente,
                        clientes[indice_nuevo].ip,
                        clientes[indice_nuevo].puerto
                    );

                    enviar_texto(socket_nuevo, "OK conectado al broker tcp\n");
                }
            }
        }

        // mensajes de clientes
        for (i = 0; i < MAXIMO_CLIENTES; i++) {
            ssize_t bytes_recibidos;

            if (!clientes[i].activo) {
                continue;
            }

            if (!FD_ISSET(clientes[i].socket_cliente, &conjunto_lectura)) {
                continue;
            }

            memset(buffer, 0, sizeof(buffer));

            bytes_recibidos = recv(
                clientes[i].socket_cliente,
                buffer,
                sizeof(buffer) - 1,
                0
            );

            if (bytes_recibidos < 0) {
                perror("recv");
                eliminar_cliente(i);
                continue;
            }

            if (bytes_recibidos == 0) {
                eliminar_cliente(i);
                continue;
            }

            buffer[bytes_recibidos] = '\0';

            printf("socket=%d envio: %s", clientes[i].socket_cliente, buffer);
            if (buffer[strlen(buffer) - 1] != '\n') {
                printf("\n");
            }

            procesar_mensaje_cliente(i, buffer);
        }
    }

    cerrar_todo_y_salir(0);
}