#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_TAMANO 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Uso: %s <IP_Intermediario> <Puerto_Intermediario> <Partido_a_Suscribir>\n", argv[0]);
        printf("Ejemplo: %s 127.0.0.1 8080 EquipoA_vs_EquipoB\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *ip_intermediario = argv[1];
    int puerto_intermediario = atoi(argv[2]);
    char *nombre_partido = argv[3];

    int descriptor_socket;
    struct sockaddr_in direccion_intermediario;
    char bufer[MAX_TAMANO];

    if ((descriptor_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    memset(&direccion_intermediario, 0, sizeof(direccion_intermediario));
    direccion_intermediario.sin_family = AF_INET;
    direccion_intermediario.sin_port = htons(puerto_intermediario);
    direccion_intermediario.sin_addr.s_addr = inet_addr(ip_intermediario);

    snprintf(bufer, sizeof(bufer), "SUB|%s", nombre_partido);
    sendto(descriptor_socket, (const char *)bufer, strlen(bufer), MSG_CONFIRM, 
          (const struct sockaddr *)&direccion_intermediario, sizeof(direccion_intermediario));
          
    printf("[SUSCRIPTOR] Suscrito al partido: %s. Esperando actualizaciones...\n", nombre_partido);

    socklen_t tamano_direccion = sizeof(direccion_intermediario);
    while (1) {
        memset(bufer, 0, MAX_TAMANO);
        int bytes_recibidos = recvfrom(descriptor_socket, (char *)bufer, MAX_TAMANO, MSG_WAITALL, 
                                      (struct sockaddr *)&direccion_intermediario, &tamano_direccion);
        if (bytes_recibidos > 0) {
            bufer[bytes_recibidos] = '\0';
            printf("\n--- ACTUALIZACION EN VIVO ---\n");
            printf("Partido: %s\n", nombre_partido);
            printf("Evento: %s\n", bufer);
            printf("-----------------------------\n");
        }
    }

    close(descriptor_socket);
    return 0;
}