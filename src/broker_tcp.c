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

static int socket_servidor = -1;
static Cliente clientes[MAXIMO_CLIENTES];

void cerrar_todo_y_salir(int codigo) {
    int i;

    if (socket_servidor != -1) {
        close(socket_servidor);
    }

    for (i = 0; i < MAXIMO_CLIENTES; i++) {
        if (clientes[i].activo) {
            close(clientes[i].socket_cliente);
            clientes[i].activo = 0;
        }
    }

    exit(codigo);
}

void manejar_ctrl_c(int senal) {
    (void)senal;
    printf("\ncerrando broker tcp...\n");
    cerrar_todo_y_salir(0);
}

void inicializar_clientes(void) {
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
    int i;

    for (i = 0; i < MAXIMO_CLIENTES; i++) {
        if (!clientes[i].activo) {
            clientes[i].socket_cliente = socket_nuevo;
            clientes[i].activo = 1;
            clientes[i].es_suscriptor = 0;
            clientes[i].tema[0] = '\0';

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
    if (indice < 0 || indice >= MAXIMO_CLIENTES || !clientes[indice].activo) {
        return;
    }

    printf(
        "cliente desconectado: socket=%d %s:%d\n",
        clientes[indice].socket_cliente,
        clientes[indice].ip,
        clientes[indice].puerto
    );

    close(clientes[indice].socket_cliente);
    clientes[indice].socket_cliente = -1;
    clientes[indice].activo = 0;
    clientes[indice].es_suscriptor = 0;
    clientes[indice].tema[0] = '\0';
    clientes[indice].ip[0] = '\0';
    clientes[indice].puerto = 0;
}

void enviar_texto(int socket_destino, const char *texto) {
    if (send(socket_destino, texto, strlen(texto), 0) < 0) {
        perror("send");
    }
}

void registrar_suscripcion(int indice, const char *tema) {
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
    int i;
    int entregados = 0;
    char mensaje_salida[TAMANO_BUFFER];
    char respuesta[TAMANO_BUFFER];

    snprintf(mensaje_salida, sizeof(mensaje_salida), "MSG %s %s\n", tema, mensaje);

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