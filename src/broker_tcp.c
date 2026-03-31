#include <stdio.h> // printf, perror
#include <stdlib.h> // exit
#include <unistd.h> // close
#include <sys/socket.h> // socket, bind, listen
#include <netinet/in.h> // sockaddr_in, htons, INADDR_ANY

#define PUERTO 53222 // puerto fijo del broker
#define BACKLOG 5 // máximo de conexiones pendientes en cola para hacer accept
#include <stdio.h> // printf, perror
#include <stdlib.h> // exit
#include <unistd.h> // close
#include <sys/socket.h> // socket, bind, listen
#include <netinet/in.h> // sockaddr_in, htons, INADDR_ANY

#define PUERTO 53222 // puerto fijo del broker
#define BACKLOG 5 // máximo de conexiones pendientes en cola para hacer accept
#define MAX_CLIENTES 10 // máximo de clientes simultáneos
#define TAM_BUFFER 1024 // tamaño del buffer de lectura

int main() {

    int socket_listen; // descriptor del socket de escucha
    
    int resultado_bind; // guardara 0 si bind sale bien y -1 si falla
    int resultado_listen; // guardara 0 si listen sale bien y -1 si falla

    int clientes[MAX_CLIENTES]; // sockets de clientes

    struct sockaddr_in direccion_broker = {0}; // direccion local del broker
    struct sockaddr *direccion_generica_broker; // puntero generico a la misma direccion

    socket_listen = socket(AF_INET, SOCK_STREAM, 0); // crea un socket TCP sobre IPv4
    if (socket_listen < 0) { perror("socket"); exit(1); } // si falla, muestra error y termina

    direccion_broker.sin_family = AF_INET; // indica que esta direccion es IPv4
    direccion_broker.sin_addr.s_addr = INADDR_ANY; // escucha por cualquier IP local de esta maquina
    direccion_broker.sin_port = htons(PUERTO); // guarda el puerto en formato de red

    direccion_generica_broker = (struct sockaddr *)&direccion_broker; // mismo bloque de memoria, visto como direccion generica

    resultado_bind = bind(socket_listen, direccion_generica_broker, sizeof(direccion_broker)); // asocia el socket con la IP y puerto locales
    if (resultado_bind < 0) { perror("bind"); close(socket_listen); exit(1); } // si falla, cierra el socket ya creado y termina

    resultado_listen = listen(socket_listen, BACKLOG); // activa la escucha y permite hasta cinco conexiones pendientes en cola
    if (resultado_listen < 0) { perror("listen"); close(socket_listen); exit(1); } // si falla, cierra el socket ya creado y termina

    printf("Broker escuchando en el puerto %d\n", PUERTO); // confirma que el broker quedo listo

    
    close(socket_listen); // cierra el socket antes de terminar el programa
    return 0; // termina correctamente
}

/*
sockaddr_in viene de <netinet/in.h> y representa una direccion IPv4 de socket.

struct in_addr {
    uint32_t s_addr; // IP en binario
};

struct sockaddr_in {
    sa_family_t sin_family; // familia, por ejemplo AF_INET
    in_port_t sin_port; // puerto en formato de red
    struct in_addr sin_addr; // IP
    unsigned char sin_zero[8]; // relleno interno
};

bind no recibe sockaddr_in*, sino sockaddr*.
sockaddr es la version generica de una direccion de socket, porque bind no solo recibe direcciones IPv4
A bind le doy un puntero a memoria y un tamaño, esa parte de la memoria contienen sockaddr_in y es la que dice IPv4

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

Entonces armamos la direccion como sockaddr_in porque es comoda para IPv4,
y luego hacemos cast a sockaddr* porque asi esta definida la API de sockets.
Ese cast no cambia los datos; solo cambia el tipo con que se los pasamos a bind.
*/
int main() {

    int socket_listen; // descriptor del socket de escucha
    
    int resultado_bind; // guardara 0 si bind sale bien y -1 si falla
    int resultado_listen; // guardara 0 si listen sale bien y -1 si falla

    struct sockaddr_in direccion_broker = {0}; // direccion local del broker
    struct sockaddr *direccion_generica_broker; // puntero generico a la misma direccion

    socket_listen = socket(AF_INET, SOCK_STREAM, 0); // crea un socket TCP sobre IPv4
    if (socket_listen < 0) { perror("socket"); exit(1); } // si falla, muestra error y termina

    direccion_broker.sin_family = AF_INET; // indica que esta direccion es IPv4
    direccion_broker.sin_addr.s_addr = INADDR_ANY; // escucha por cualquier IP local de esta maquina
    direccion_broker.sin_port = htons(PUERTO); // guarda el puerto en formato de red

    direccion_generica_broker = (struct sockaddr *)&direccion_broker; // mismo bloque de memoria, visto como direccion generica

    resultado_bind = bind(socket_listen, direccion_generica_broker, sizeof(direccion_broker)); // asocia el socket con la IP y puerto locales
    if (resultado_bind < 0) { perror("bind"); close(socket_listen); exit(1); } // si falla, cierra el socket ya creado y termina

    resultado_listen = listen(socket_listen, BACKLOG); // activa la escucha y permite hasta cinco conexiones pendientes en cola
    if (resultado_listen < 0) { perror("listen"); close(socket_listen); exit(1); } // si falla, cierra el socket ya creado y termina

    printf("Broker escuchando en el puerto %d\n", PUERTO); // confirma que el broker quedo listo


    close(socket_listen); // cierra el socket antes de terminar el programa
    return 0; // termina correctamente
}

/*
sockaddr_in viene de <netinet/in.h> y representa una direccion IPv4 de socket.

struct in_addr {
    uint32_t s_addr; // IP en binario
};

struct sockaddr_in {
    sa_family_t sin_family; // familia, por ejemplo AF_INET
    in_port_t sin_port; // puerto en formato de red
    struct in_addr sin_addr; // IP
    unsigned char sin_zero[8]; // relleno interno
};

bind no recibe sockaddr_in*, sino sockaddr*.
sockaddr es la version generica de una direccion de socket, porque bind no solo recibe direcciones IPv4
A bind le doy un puntero a memoria y un tamaño, esa parte de la memoria contienen sockaddr_in y es la que dice IPv4

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

Entonces armamos la direccion como sockaddr_in porque es comoda para IPv4,
y luego hacemos cast a sockaddr* porque asi esta definida la API de sockets.
Ese cast no cambia los datos; solo cambia el tipo con que se los pasamos a bind.
*/