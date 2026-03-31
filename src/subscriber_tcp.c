// cliente suscriptor
// socket -> connect
// send -> recv
// inicia la conexion, se declara como subscriber, luego el usuario decide a que topicos suscribirse y queda escuchando mensajes

#include <arpa/inet.h> // inet_pton
#include <netinet/in.h> // sockaddr_in, htons
#include <stdio.h> // printf, perror, fgets
#include <stdlib.h> // exit
#include <string.h> // strlen, memset, strcspn
#include <sys/socket.h> // socket, connect, recv, send
#include <unistd.h> // close

#define PUERTO_BROKER 5222 // puerto actual del broker
#define TAMANO_BUFFER 2048 // buffer temporal para mensajes

int main(void) {
    int socket_cliente; // descriptor del socket del subscriber
    struct sockaddr_in direccion_broker; // direccion del broker
    char buffer[TAMANO_BUFFER]; // buffer para mensajes entrantes y salientes
    const char *ip_broker = "127.0.0.1"; // IP local del broker para pruebas
    ssize_t bytes_recibidos; // cantidad de bytes que devuelve recv

    socket_cliente = socket(AF_INET, SOCK_STREAM, 0); // crea socket TCP del subscriber
    if (socket_cliente < 0) {
        perror("socket");
        return 1;
    }

    memset(&direccion_broker, 0, sizeof(direccion_broker)); // limpia la estructura de direccion
    direccion_broker.sin_family = AF_INET; // indica IPv4
    direccion_broker.sin_port = htons(PUERTO_BROKER); // guarda el puerto en formato de red

    if (inet_pton(AF_INET, ip_broker, &direccion_broker.sin_addr) <= 0) { // convierte la IP del broker a formato binario
        perror("inet_pton");
        close(socket_cliente);
        return 1;
    }

    if (connect(socket_cliente, (struct sockaddr *)&direccion_broker, sizeof(direccion_broker)) < 0) { // se conecta al broker
        perror("connect");
        close(socket_cliente);
        return 1;
    }

    printf("Conectado al broker TCP en %s:%d\n", ip_broker, PUERTO_BROKER); // confirma la conexion

    memset(buffer, 0, sizeof(buffer)); // limpia buffer antes de leer
    bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0); // lee saludo inicial del broker
    if (bytes_recibidos > 0) {
        buffer[bytes_recibidos] = '\0'; // agrega fin de string
        printf("%s", buffer); // imprime respuesta del broker
    }

    if (send(socket_cliente, "ROLE SUBSCRIBER\n", strlen("ROLE SUBSCRIBER\n"), 0) < 0) { // envia el rol subscriber por defecto
        perror("send");
        close(socket_cliente);
        return 1;
    }

    printf("Rol enviado: ROLE SUBSCRIBER\n"); // confirma lo que se mando

    memset(buffer, 0, sizeof(buffer)); // limpia buffer antes de leer
    bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0); // lee confirmacion del broker
    if (bytes_recibidos > 0) {
        buffer[bytes_recibidos] = '\0'; // agrega fin de string
        printf("%s", buffer); // imprime confirmacion
    }

    printf("Ya puedes escribir comandos como:\n"); // muestra guia basica
    printf("  SUB Santa_Fe_vs_Millonarios\n");
    printf("  SUB Uniandes_vs_Javeriana\n");
    printf("  SUB Colombia_vs_Francia\n");
    printf("  EXIT\n");

    while (1) {
        fd_set sockets_a_vigilar; // conjunto de sockets que select revisara
        int mayor_socket; // mayor descriptor para select
        int resultado_select; // resultado de select

        FD_ZERO(&sockets_a_vigilar); // limpia el conjunto
        FD_SET(socket_cliente, &sockets_a_vigilar); // agrega el socket del broker para recibir mensajes
        FD_SET(STDIN_FILENO, &sockets_a_vigilar); // agrega teclado para que el usuario pueda escribir comandos
        mayor_socket = socket_cliente; // empieza suponiendo que el mayor descriptor es el socket

        if (STDIN_FILENO > mayor_socket) { // compara si el descriptor del teclado es mayor
            mayor_socket = STDIN_FILENO; // actualiza el mayor descriptor si hace falta
        }

        resultado_select = select(mayor_socket + 1, &sockets_a_vigilar, NULL, NULL, NULL); // espera actividad del broker o del teclado
        if (resultado_select < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &sockets_a_vigilar)) { // revisa si el usuario escribió algo por teclado
            memset(buffer, 0, sizeof(buffer)); // limpia buffer antes de leer
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) { // lee una linea desde teclado
                printf("Entrada finalizada\n");
                break;
            }

            if (send(socket_cliente, buffer, strlen(buffer), 0) < 0) { // envia el comando al broker
                perror("send");
                break;
            }

            if (strncmp(buffer, "EXIT", 4) == 0) { // si el usuario pidio salir, se espera la respuesta del broker y luego se termina
                continue;
            }
        }

        if (FD_ISSET(socket_cliente, &sockets_a_vigilar)) { // revisa si el broker envio algo
            memset(buffer, 0, sizeof(buffer)); // limpia buffer antes de leer
            bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0); // lee mensaje del broker

            if (bytes_recibidos < 0) {
                perror("recv");
                break;
            }

            if (bytes_recibidos == 0) {
                printf("El broker cerro la conexion\n");
                break;
            }

            buffer[bytes_recibidos] = '\0'; // agrega fin de string
            printf("%s", buffer); // imprime mensaje del broker

            if (strncmp(buffer, "BYE", 3) == 0) { // si el broker respondio BYE, termina ordenadamente
                break;
            }
        }
    }

    close(socket_cliente); // cierra el socket antes de terminar
    return 0;
}