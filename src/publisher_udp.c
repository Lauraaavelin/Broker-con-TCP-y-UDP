#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_TAMANO 1024

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Uso: %s <IP_Intermediario> <Puerto_Intermediario> <Partido> \"<Mensaje>\"\n", argv[0]);
        printf("Ejemplo: %s 127.0.0.1 8080 EquipoA_vs_EquipoB \"Gol al minuto 32\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *ip_intermediario = argv[1];
    int puerto_intermediario = atoi(argv[2]);
    char *nombre_partido = argv[3];
    char *mensaje_evento = argv[4];

    int descriptor_socket;
    struct sockaddr_in direccion_intermediario;

    if ((descriptor_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    memset(&direccion_intermediario, 0, sizeof(direccion_intermediario));
    direccion_intermediario.sin_family = AF_INET;
    direccion_intermediario.sin_port = htons(puerto_intermediario);
    direccion_intermediario.sin_addr.s_addr = inet_addr(ip_intermediario);

    char bufer[MAX_TAMANO];
    snprintf(bufer, sizeof(bufer), "PUB|%s|%s", nombre_partido, mensaje_evento);

    sendto(descriptor_socket, (const char *)bufer, strlen(bufer), MSG_CONFIRM, 
          (const struct sockaddr *)&direccion_intermediario, sizeof(direccion_intermediario));
          
    printf("[PUBLICADOR] Mensaje enviado al intermediario: %s\n", bufer);

    close(descriptor_socket);
    return 0;
}