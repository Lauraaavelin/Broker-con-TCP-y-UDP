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
#include <stdbool.h>

#define PUERTO_BROKER 5000      // puerto del broker
#define TAMANO_BUFFER 1024      // buffer temporal para mensajes
const char *TOPICOS_VALIDOS[3] = {
    "Santa_Fe_vs_Millonarios",
    "Uniandes_vs_Javeriana",
    "Colombia_vs_Francia"
}; // lista fija de tópicos permitidos por el broker
#define TOTAL_MENSAJES 10       // minimo pedido por el laboratorio

int main(int argc, char *argv[]) {
    int socket_cliente;
    int i;
    struct sockaddr_in direccion_broker;
    char buffer[TAMANO_BUFFER];
    char publicacion[TAMANO_BUFFER];
    const char *ip_broker = "127.0.0.1";
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
    //TODO esto hay que cambiarlo para que el publicador publique lo que esta en el archivo txt en lugar de generar mensajes de ejemplo
    // en el txt hay un mensaje por linea con el formato "TOPICO: MENSAJE", el publicador debe leer cada linea, extraer el topico y el mensaje, y enviar una publicacion al broker con ese formato, ademas de imprimir en consola lo que esta publicando
    // El usuario solo ingresa el nombre del archivo txt por consola despues de ejecutar el programa, y el programa debe abrir ese archivo, leerlo linea por linea, y publicar cada mensaje al broker con el formato adecuado, ademas de imprimir en consola lo que esta publicando.
  



    bool publicar_mas = true;

    while (publicar_mas) {
        char nombre_archivo[256];
        printf("Ingrese el nombre del archivo: ");
        scanf("%255s", nombre_archivo);

        FILE *archivo = fopen(nombre_archivo, "r");
        if (!archivo) {
            perror("fopen");
            continue;
        }

        char linea[TAMANO_BUFFER];

        while (fgets(linea, sizeof(linea), archivo)) {

            char *sep = strchr(linea, ':');
            if (!sep) continue;

            *sep = '\0';
            char *topico = linea;
            char *mensaje = sep + 1;

            while (*mensaje == ' ') mensaje++;
            mensaje[strcspn(mensaje, "\r\n")] = '\0';

            snprintf(publicacion, sizeof(publicacion),
                    "PUB %s %s\n", topico, mensaje);

            if (send(socket_cliente, publicacion, strlen(publicacion), 0) < 0) {
                perror("send");
                break;
            }

            printf("Publicado: %s", publicacion);

            // leer respuesta del broker
            ssize_t n = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';
                printf("Broker: %s", buffer);
            }
        }

        fclose(archivo);

        char opcion[5];
        printf("¿Desea continuar? (s/n): ");
        scanf("%9s", opcion);

        publicar_mas = (strcmp(opcion, "s") == 0);
    }
    close(socket_cliente);
    return 0;
}