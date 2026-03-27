// cliente publicador
// socket -> connect
// send -> recv (inicia la conexion, publica mensajes y recibe respuesta)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PUERTO_BROKER 5000      // puerto del broker
#define TAMANO_BUFFER 1024      // buffer temporal para mensajes
#define TEMA_POR_DEFECTO "partido1"
#define TOTAL_MENSAJES 10       // minimo pedido por el laboratorio

int main(int argc, char *argv[]) {
    int socket_cliente;
    int i;
    struct sockaddr_in direccion_broker;
    char buffer[TAMANO_BUFFER];
    char publicacion[TAMANO_BUFFER];
    const char *ip_broker = "127.0.0.1";
    const char *tema = TEMA_POR_DEFECTO;
    ssize_t bytes_recibidos;

    if (argc >= 2) {
        tema = argv[1];
    }

    // crear socket tcp del publicador
    socket_cliente = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_cliente < 0) {
        perror("socket");
        return 1;
    }

    memset(&direccion_broker, 0, sizeof(direccion_broker));
    direccion_broker.sin_family = AF_INET;
    direccion_broker.sin_port = htons(PUERTO_BROKER);

    if (inet_pton(AF_INET, ip_broker, &direccion_broker.sin_addr) <= 0) {
        perror("inet_pton");
        close(socket_cliente);
        return 1;
    }

    // conectarse al broker
    if (connect(
            socket_cliente,
            (struct sockaddr *)&direccion_broker,
            sizeof(direccion_broker)
        ) < 0) {
        perror("connect");
        close(socket_cliente);
        return 1;
    }

    printf("conectado al broker tcp en %s:%d\n", ip_broker, PUERTO_BROKER);

    // leer mensaje inicial del broker
    memset(buffer, 0, sizeof(buffer));
    bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);
    if (bytes_recibidos > 0) {
        buffer[bytes_recibidos] = '\0';
        printf("%s", buffer);
    }

    // enviar 10 publicaciones
    for (i = 1; i <= TOTAL_MENSAJES; i++) {
        snprintf(
            publicacion,
            sizeof(publicacion),
            "PUB %s Gol del equipo A minuto %d\n",
            tema,
            i * 5
        );

        if (send(socket_cliente, publicacion, strlen(publicacion), 0) < 0) {
            perror("send");
            close(socket_cliente);
            return 1;
        }

        printf("publicacion enviada: %s", publicacion);

        memset(buffer, 0, sizeof(buffer));
        bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);
        if (bytes_recibidos < 0) {
            perror("recv");
            close(socket_cliente);
            return 1;
        }

        if (bytes_recibidos == 0) {
            printf("el broker cerro la conexion\n");
            close(socket_cliente);
            return 1;
        }

        buffer[bytes_recibidos] = '\0';
        printf("%s", buffer);

        sleep(1);
    }

    close(socket_cliente);
    return 0;
}