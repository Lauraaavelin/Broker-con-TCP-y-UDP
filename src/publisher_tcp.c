// cliente publicador
// socket -> connect
// send -> recv
// inicia la conexion, se declara como publisher por defecto, publica mensajes leidos desde un archivo y recibe respuesta del broker

#include <arpa/inet.h> // inet_pton
#include <netinet/in.h> // sockaddr_in, htons
#include <stdio.h> // printf, perror, fgets, FILE, fopen, fclose
#include <stdlib.h> // return
#include <string.h> // strlen, strcmp, strcspn, strchr, memset
#include <sys/socket.h> // socket, connect, recv, send
#include <unistd.h> // close
#include <stdbool.h> // bool, true, false

#define PUERTO_BROKER 5222 // puerto actual del broker
#define TAMANO_BUFFER 2048 // buffer temporal para mensajes

int main(void) {
    int socket_cliente; // descriptor del socket del publicador
    struct sockaddr_in direccion_broker; // direccion del broker
    char buffer[TAMANO_BUFFER]; // buffer para mensajes de entrada y salida
    char publicacion[TAMANO_BUFFER]; // buffer para construir cada comando PUB
    const char *ip_broker = "127.0.0.1"; // IP local del broker para pruebas
    ssize_t bytes_recibidos; // cantidad de bytes que devuelve recv
    bool publicar_mas = true; // controla si el usuario quiere seguir publicando archivos

    socket_cliente = socket(AF_INET, SOCK_STREAM, 0); // crea socket TCP del publicador
    if (socket_cliente < 0) {
        perror("socket");
        return 1;
    }

    memset(&direccion_broker, 0, sizeof(direccion_broker)); // limpia la estructura de direccion
    direccion_broker.sin_family = AF_INET; // indica IPv4
    direccion_broker.sin_port = htons(PUERTO_BROKER); // guarda el puerto en formato de red

    if (inet_pton(AF_INET, ip_broker, &direccion_broker.sin_addr) <= 0) { // convierte la IP del broker a binario
        perror("inet_pton");
        close(socket_cliente);
        return 1;
    }

    if (connect(socket_cliente, (struct sockaddr *)&direccion_broker, sizeof(direccion_broker)) < 0) { // conecta el publicador con el broker
        perror("connect");
        close(socket_cliente);
        return 1;
    }

    printf("Conectado al broker TCP en %s:%d\n", ip_broker, PUERTO_BROKER); // confirma la conexion

    memset(buffer, 0, sizeof(buffer)); // limpia buffer antes de leer
    bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0); // lee saludo inicial del broker
    if (bytes_recibidos > 0) {
        buffer[bytes_recibidos] = '\0'; // agrega fin de string
        printf("%s", buffer); // imprime mensaje del broker
    }

    if (send(socket_cliente, "ROLE PUBLISHER\n", strlen("ROLE PUBLISHER\n"), 0) < 0) { // envia el rol publisher por defecto al iniciar
        perror("send");
        close(socket_cliente);
        return 1;
    }

    printf("Rol enviado: ROLE PUBLISHER\n"); // confirma que ya se mando el rol al broker

    memset(buffer, 0, sizeof(buffer)); // limpia buffer antes de leer
    bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0); // lee confirmacion del broker
    if (bytes_recibidos > 0) {
        buffer[bytes_recibidos] = '\0'; // agrega fin de string
        printf("%s", buffer); // imprime confirmacion del broker
    }

    while (publicar_mas) { // permite publicar varios archivos sin cerrar la conexion

        char nombre_archivo[256]; // nombre del archivo ingresado por el usuario
        FILE *archivo; // puntero al archivo que se va a leer
        char linea[TAMANO_BUFFER]; // linea leida del archivo
        char opcion[10]; // respuesta del usuario para seguir o no

        printf("Ingrese el nombre del archivo: "); // pide el nombre del archivo al usuario
        if (scanf("%255s", nombre_archivo) != 1) { // lee el nombre del archivo
            printf("No se pudo leer el nombre del archivo\n");
            break;
        }

        archivo = fopen(nombre_archivo, "r"); // abre el archivo en modo lectura
        if (!archivo) {
            perror("fopen");
            continue;
        }

        while (fgets(linea, sizeof(linea), archivo)) { // lee el archivo linea por linea

            char *separador; // apuntara al caracter :
            char *topico; // apuntara al topico extraido de la linea
            char *mensaje; // apuntara al mensaje extraido de la linea

            separador = strchr(linea, ':'); // busca el caracter : para separar topico y mensaje
            if (!separador) { // si no hay :, el formato de la linea esta mal
                printf("Linea ignorada por formato invalido: %s", linea);
                continue;
            }

            *separador = '\0'; // corta la linea en dos partes: topico y mensaje
            topico = linea; // la parte izquierda queda como topico
            mensaje = separador + 1; // la parte derecha queda como mensaje

            while (*mensaje == ' ') { // salta espacios iniciales antes del mensaje
                mensaje++;
            }

            topico[strcspn(topico, "\r\n")] = '\0'; // elimina salto de linea del topico si lo hubiera
            mensaje[strcspn(mensaje, "\r\n")] = '\0'; // elimina salto de linea del mensaje

            size_t max_mensaje = sizeof(publicacion)
                            - strlen("PUB ")
                            - strlen(topico)
                            - strlen("\n")
                            - 1;

            if (strlen(mensaje) > max_mensaje) {
                mensaje[max_mensaje] = '\0';
            }

            snprintf(publicacion, sizeof(publicacion), "PUB %s %s\n", topico, mensaje);
            if (send(socket_cliente, publicacion, strlen(publicacion), 0) < 0) { // envia la publicacion al broker
                perror("send");
                break;
            }

            printf("Publicado: %s", publicacion); // muestra lo que se envio al broker

            memset(buffer, 0, sizeof(buffer)); // limpia el buffer antes de leer respuesta
            bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0); // lee respuesta del broker
            if (bytes_recibidos > 0) {
                buffer[bytes_recibidos] = '\0'; // agrega fin de string
                printf("Broker: %s", buffer); // muestra la respuesta del broker
            } else if (bytes_recibidos == 0) {
                printf("El broker cerro la conexion\n");
                fclose(archivo);
                close(socket_cliente);
                return 0;
            } else {
                perror("recv");
                fclose(archivo);
                close(socket_cliente);
                return 1;
            }
        }

        fclose(archivo); // cierra el archivo una vez termina de leerlo completo

        printf("Desea continuar publicando otro archivo? (s/n): "); // pregunta si quiere seguir
        if (scanf("%9s", opcion) != 1) { // lee respuesta del usuario
            printf("No se pudo leer la opcion\n");
            break;
        }

        if (strcmp(opcion, "s") == 0 || strcmp(opcion, "S") == 0) { // sigue publicando si responde s o S
            publicar_mas = true;
        } else {
            publicar_mas = false; // termina el ciclo si responde distinto
        }
    }

    if (send(socket_cliente, "EXIT\n", strlen("EXIT\n"), 0) < 0) { // intenta cerrar ordenadamente la sesion en el broker
        perror("send");
    } else {
        memset(buffer, 0, sizeof(buffer)); // limpia buffer antes de leer
        bytes_recibidos = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0); // lee respuesta BYE del broker
        if (bytes_recibidos > 0) {
            buffer[bytes_recibidos] = '\0'; // agrega fin de string
            printf("%s", buffer); // imprime respuesta final del broker
        }
    }

    close(socket_cliente); // cierra el socket antes de terminar
    return 0;
}
