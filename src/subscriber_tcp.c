// cliente suscriptor
// socket -> connect
// send -> recv (inicia la conexion, se suscribe y espera mensajes)

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

int main(int argc, char *argv[]) {
    int socket_cliente;
    struct sockaddr_in direccion_broker;
    char buffer[TAMANO_BUFFER];
    char suscripcion[TAMANO_BUFFER];
    const char *ip_broker = "127.0.0.1";
    const char *tema = TEMA_POR_DEFECTO;
    ssize_t bytes_recibidos;

    if (argc >= 2) {
        tema = argv[1];
    }

    // crear socket tcp del suscriptor
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

    // enviar suscripcion
    snprintf(suscripcion, sizeof(suscripcion), "SUB %s\n", tema);
    if (send(socket_cliente, suscripcion, strlen(suscripcion), 0) < 0) {
        perror("send");
        close(socket_cliente);
        return 1;
    }

    printf("suscripcion enviada: %s", suscripcion);

    // leer ack de suscripcion
    memset(buffer, 0, sizeof(buffer));
    bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);
    if (bytes_recibidos > 0) {
        buffer[bytes_recibidos] = '\0';
        printf("%s", buffer);
    }

    // quedar escuchando mensajes reenviados por el broker
    while (1) {
        memset(buffer, 0, sizeof(buffer));

        bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);
        if (bytes_recibidos < 0) {
            perror("recv");
            break;
        }

        if (bytes_recibidos == 0) {
            printf("el broker cerro la conexion\n");
            break;
        }

        buffer[bytes_recibidos] = '\0';
        printf("mensaje recibido: %s", buffer);
    }

    close(socket_cliente);
    return 0;
}