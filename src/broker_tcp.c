#include <stdio.h>          // printf, perror
#include <stdlib.h>         // exit
#include <string.h>         // memset
#include <unistd.h>         // close
#include <sys/socket.h>     // socket, bind, listen
#include <netinet/in.h>     // sockaddr_in
#include <arpa/inet.h>      // funciones IP

#define PUERTO 54321 // puerto en el que el broker escuchará
#define BACKLOG 5 // número máximo de conexiones en espera para el accept

int main() {

    int socket_listen; // descriptor/identificador del socket de escucha del broker
    struct sockaddr_in direccion; // dirección (IP + puerto)

    int res;

    socket_listen = socket(AF_INET, SOCK_STREAM, 0); // crea socket TCP IPv4 y devuele su descriptor
    if (socket_listen < 0) { perror("socket"); exit(1); } // si hubo error al crear el socket, muestra mensaje etiquetado con socket y termina

    memset(&direccion, 0, sizeof(direccion)); // limpia la estructura, llena con ceros

    direccion.sin_family = AF_INET;            // IPv4
    direccion.sin_addr.s_addr = INADDR_ANY;    // cualquier IP local
    direccion.sin_port = htons(PUERTO);        // puerto en formato de red

    res = bind(socket_listen, (struct sockaddr*)&direccion, sizeof(direccion)); // asocia socket a IP/puerto
    if (res < 0) { perror("bind"); close(socket_listen); exit(1); }

    res = listen(socket_listen, BACKLOG);  // pone en modo escucha
    if (res < 0) { perror("listen"); close(socket_listen); exit(1); }

    printf("Broker escuchando en puerto %d\n", PUERTO);

    close(socket_listen);                  // cierra socket

    return 0;
}