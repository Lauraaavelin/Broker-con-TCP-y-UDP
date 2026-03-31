#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define MAX_TAMANO 1024
#define MAX_SUSCRIPTORES 100

typedef struct {
    struct sockaddr_in direccion;
    char partido[50];
} Suscriptor;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <Puerto>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int puerto = atoi(argv[1]);
    int descriptor_socket;
    struct sockaddr_in direccion_servidor, direccion_cliente;
    char bufer[MAX_TAMANO];
    socklen_t tamano_direccion = sizeof(direccion_cliente);

    Suscriptor lista_suscriptores[MAX_SUSCRIPTORES];
    int total_suscriptores = 0;

    if ((descriptor_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    memset(&direccion_servidor, 0, sizeof(direccion_servidor));
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons(puerto);

    if (bind(descriptor_socket, (const struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
        perror("Error en la asignacion (bind)");
        close(descriptor_socket);
        exit(EXIT_FAILURE);
    }

    printf("Intermediario UDP escuchando en el puerto %d...\n", puerto);

    while (1) {
        memset(bufer, 0, MAX_TAMANO);
        int bytes_recibidos = recvfrom(descriptor_socket, (char *)bufer, MAX_TAMANO, MSG_WAITALL, 
                                      (struct sockaddr *)&direccion_cliente, &tamano_direccion);
        if (bytes_recibidos > 0) {
            bufer[bytes_recibidos] = '\0';
            
            if (strncmp(bufer, "SUB|", 4) == 0) {
                if (total_suscriptores < MAX_SUSCRIPTORES) {
                    char *nombre_partido = bufer + 4;
                    lista_suscriptores[total_suscriptores].direccion = direccion_cliente;
                    strncpy(lista_suscriptores[total_suscriptores].partido, nombre_partido, 50);
                    printf("[INTERMEDIARIO] Nuevo suscriptor para el partido: %s\n", nombre_partido);
                    total_suscriptores++;
                } else {
                    printf("[INTERMEDIARIO] Limite de suscriptores alcanzado.\n");
                }
            } 
            else if (strncmp(bufer, "PUB|", 4) == 0) {
                char *nombre_partido = strtok(bufer + 4, "|");
                char *contenido_mensaje = strtok(NULL, "");

                if (nombre_partido != NULL && contenido_mensaje != NULL) {
                    printf("[INTERMEDIARIO] De publicador - Partido: %s -> Mensaje: %s\n", nombre_partido, contenido_mensaje);
                    
                    for (int i = 0; i < total_suscriptores; i++) {
                        if (strcmp(lista_suscriptores[i].partido, nombre_partido) == 0) {
                            sendto(descriptor_socket, contenido_mensaje, strlen(contenido_mensaje), MSG_CONFIRM, 
                                  (const struct sockaddr *)&lista_suscriptores[i].direccion, tamano_direccion);
                        }
                    }
                }
            }
        }
    }

    close(descriptor_socket);
    return 0;
}