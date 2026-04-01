# Lab 3: Sockets TCP y UDP

Este proyecto consiste en una plataforma de distribución de noticias de fútbol en tiempo real. Utiliza un modelo de Publicación-Suscripción donde un nodo central (Broker) gestiona el flujo de información entre periodistas (Publishers) y aficionados (Subscribers). Tiene dos versiones TCP y UDP para hacer pruebas de rendimiento.

## 1. Integrantes
- Laura Avelino
- Alejandro Franco
- Kevin Hernandez 
- Laura Martinez
 
## 2. Sockets UDP

# Compilar los archivos y Ejecutar en Terminales (UDP):

#### Paso 1: Genera los ejecutables desde tu directorio de trabajo:

    gcc broker_udp.c -o broker
    gcc publisher_udp.c -o publisher
    gcc subscriber_udp.c -o subscriber

    TODAS LAS TERMINALES DEBEN ESTAR SOBRE EL DIRECTORIO DE TRABAJO DONDE ESTAN LOS EJECUTABLES.

#### Paso 2: Lanzar el Broker (Terminal 1)
    Este será el corazón del sistema. Debe estar encendido siempre antes que los demás.
    Ejecutar en la Terminal: ./broker 8080

#### Paso 3: Suscribir a los hinchas (Terminales 2 y 3)
    Imagina que hay dos personas viendo partidos distintos.

    En la Terminal 2 (Seguir a Colombia):
    Ejecutar en la Terminal: ./subscriber 127.0.0.1 8080 Colombia_vs_Brasil

    En la Terminal 3 (Seguir a Argentina):
    Ejecutar en la Terminal: ./subscriber 127.0.0.1 8080 Argentina_vs_Chile

#### Paso 4: Publicar las noticias (Terminales 4 y 5)
    Ahora los periodistas informan lo que pasa en los estadios.

    En la Terminal 4 (Gol en el partido de Colombia):
    Ejecutar en la Terminal: ./publisher 127.0.0.1 8080 Colombia_vs_Brasil "¡Gol de Luis Díaz al minuto 20!"

    En la Terminal 5 (Tarjeta en el partido de Argentina):
    Ejecutar en la Terminal: ./publisher 127.0.0.1 8080 Argentina_vs_Chile "Tarjeta amarilla para Messi"

# Documentacion Librerias (UDP):

    socket(): El programa le pide al sistema operativo que abra un canal de comunicación. Usamos el dominio AF_INET para internet y SOCK_DGRAM porque UDP no requiere una conexión fija, devolviendo un identificador para manejar el tráfico.

    close(): Al terminar la ejecución, esta función libera el identificador del socket. Esto es fundamental para que el puerto no quede "bloqueado" y el sistema operativo pueda reutilizarlo inmediatamente.

    memset(): Antes de asignar una dirección, llenamos la estructura de memoria con ceros. Esto evita que queden datos "basura" de otros procesos que podrían causar que los mensajes se envíen a una dirección equivocada.

    sockaddr_in: Es el contenedor donde guardamos la IP y el puerto. Actúa como el sobre de una carta, indicando exactamente a dónde debe ir la información en la red.

    htons(): Los ordenadores y las redes a veces leen los números al revés (orden de bytes). Esta función traduce el número de puerto al lenguaje universal de la red para que la comunicación sea compatible.

    inet_addr(): Convierte la dirección IP que escribimos como texto (ej. "127.0.0.1") a un código binario que las tarjetas de red pueden procesar para el envío de paquetes.

    bind(): Esta función es exclusiva del Broker. Con ella, el programa "se adueña" de un puerto específico en la máquina. Así, cualquier mensaje que llegue a ese puerto será entregado directamente a nuestro proceso.

    sendto(): Como en UDP no existe una conexión permanente, cada vez que mandamos un mensaje debemos decirle a la red quién es el destinatario. Por eso, pasamos la dirección completa en cada envío.

    recvfrom(): El programa se queda esperando hasta que llega un paquete. Lo interesante es que, además de darnos el mensaje, nos dice automáticamente quién lo envió, permitiendo al Broker saber a qué dirección debe responder.

    snprintf(): Antes de enviar cualquier dato, usamos esta función para construir el mensaje combinando el tipo de comando (SUB o PUB) con el nombre del partido. Es la que da formato a nuestra comunicación antes de que salga al cable de red.


## 3. Sockets TCP
Aquí tienes una documentación clara y profunda lista para usar en tu README sobre `<sys/socket.h>`:

---

# `<sys/socket.h>` – Documentación técnica

La librería `<sys/socket.h>` proporciona la interfaz en C para utilizar sockets, que son el mecanismo estándar de comunicación entre procesos a través de red en sistemas tipo Unix (Linux, BSD, etc.). Esta librería no implementa la lógica de red por sí misma; actúa como un puente entre el espacio de usuario y el kernel, donde reside la pila de red (TCP/IP).

---

# 1. Concepto de socket

Un socket es un descriptor de archivo que representa un endpoint de comunicación. Internamente, al crear un socket, el kernel asigna una estructura que contiene:

* Tipo de socket (TCP, UDP, etc.)
* Buffers de envío y recepción
* Estado de la conexión (en TCP)
* Referencias al protocolo subyacente

Desde el programa, el socket se maneja como un entero (`int`), pero en el kernel está asociado a estructuras complejas que gestionan la comunicación.

---

# 2. Flujo general de comunicación

## Publicador

```text
socket() → bind() → listen() → accept() → recv()/send() → close()
```

## Subscriptor

```text
socket() → connect() → send()/recv() → close()
```

---

# 3. Funciones principales

## 3.1 `socket()`

```c
int socket(int domain, int type, int protocol);
```

Crea un socket y devuelve un descriptor de archivo.

* `domain`: familia de direcciones (ej. `AF_INET` para IPv4)
* `type`: tipo de socket (`SOCK_STREAM` para TCP, `SOCK_DGRAM` para UDP)
* `protocol`: normalmente 0 (el sistema selecciona el adecuado)

Internamente, el kernel:

* Reserva estructuras de socket
* Asocia el protocolo correspondiente
* Devuelve un descriptor

---

## 3.2 `bind()`

```c
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

Asocia un socket a una dirección (IP y puerto).

El kernel registra esta asociación en una tabla interna para poder enrutar conexiones entrantes.

---

## 3.3 `listen()`

```c
int listen(int sockfd, int backlog);
```

Marca un socket como pasivo (servidor) y define una cola de conexiones pendientes.

Internamente se crean:

* Cola de conexiones en proceso (SYN queue)
* Cola de conexiones establecidas (accept queue)

---

## 3.4 `accept()`

```c
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

Acepta una conexión entrante.

* No reutiliza el socket original
* Crea un nuevo descriptor para la conexión específica con el cliente

---

## 3.5 `connect()`

```c
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
```

Inicia una conexión desde el cliente hacia el servidor.

En TCP:

* Envía SYN
* Espera SYN-ACK
* Envía ACK

El socket pasa a estado `ESTABLISHED`.

---

## 3.6 `send()`

```c
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
```

Envía datos a través del socket.

Importante:

* No envía directamente a la red
* Copia los datos al buffer del kernel
* El kernel decide cuándo transmitirlos

Puede enviar menos bytes de los solicitados.

---

## 3.7 `recv()`

```c
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
```

Recibe datos desde el socket.

* Lee desde el buffer de recepción del kernel
* Copia los datos a memoria de usuario

Puede devolver:

* Menos datos que los solicitados
* 0 si la conexión fue cerrada

---

## 3.8 `close()`

```c
int close(int sockfd);
```

Cierra el socket.

En TCP:

* Inicia el cierre de conexión (envío de FIN)
* Libera recursos del kernel

---

# 4. Estructuras de datos clave

## 4.1 `struct sockaddr`

Estructura genérica para direcciones:

```c
struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};
```

Se usa como base para otras estructuras específicas.

---

## 4.2 `struct sockaddr_in` (IPv4)

```c
struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
};
```

* `sin_family`: `AF_INET`
* `sin_port`: puerto (en formato de red)
* `sin_addr`: dirección IP

---

# 5. Manejo de datos: punteros vs red

Las funciones como `send()` y `recv()` utilizan punteros, pero:

* Los punteros no se envían por la red
* Solo se copian los bytes a los que apuntan

Ejemplo:

```c
char *msg = "Hola";
send(sockfd, msg, 4, 0);
```

Lo que se envía son los bytes:

```text
48 6F 6C 61
```

El receptor reconstruye los datos en su propia memoria.

---

# 6. Buffers del kernel

Cada socket mantiene:

* Buffer de envío (send buffer)
* Buffer de recepción (recv buffer)

Flujo de datos:

```text
Programa → send() → buffer kernel → red → buffer kernel → recv() → programa
```

---

# 7. TCP vs UDP

## TCP (`SOCK_STREAM`)

* Orientado a conexión
* Garantiza entrega
* Ordena los datos
* Maneja retransmisión

## UDP (`SOCK_DGRAM`)

* Sin conexión
* No garantiza entrega
* No asegura orden

---

# 8. Consideraciones importantes

## 8.1 TCP es un flujo de bytes

No existen mensajes como tal. Ejemplo:

```c
send("Hola");
send("Mundo");
```

El receptor puede recibir:

```text
Holamundo
```

o fragmentos parciales.

---

## 8.2 `send()` no garantiza entrega inmediata

Solo indica que los datos fueron copiados al buffer del kernel.

---

## 8.3 `recv()` puede devolver menos datos

Es necesario usar bucles si se espera una cantidad específica.

---

## 8.4 Endianness

Los enteros deben convertirse a formato de red:

* `htons()`, `htonl()`
* `ntohs()`, `ntohl()`

---

# 9. Modelo conceptual

```text
Aplicación
   ↓
sys/socket.h (API)
   ↓
Kernel (buffers, estado, colas)
   ↓
TCP/UDP
   ↓
Red
```

---

# 10. Conclusión

La librería `<sys/socket.h>` no implementa la comunicación en sí, sino que proporciona una interfaz para interactuar con el subsistema de red del kernel. Comprender su funcionamiento implica entender:

* Cómo el kernel maneja buffers y estados
* Que la red transmite bytes, no estructuras ni punteros
* Que TCP es un flujo continuo, no orientado a mensajes

Con esta libreria podemos garantizar el cumplimiento del tree way handshaking, la retrasmicion y el ordenamiento de paquetes, ya que cuando la usamos esta se encarga de manejar todo en el protocolo tcp en la pila del kernell, entonces hace todo lo que se esperaria de TCP en el sistema operativo. 

---
# ¿CÓMO EJECUTAR? 

Para poder correr todo lo primero que hacemos es ubicarnos desde la consola en la carpeta bin. Con el nombre de cada uno de los archivos 
* Si estas en windos puedes usar WSL para poder  correr los archivos o puedes usar maquinas virtuales
  ```
  >> wsl
  
  >> ./broker_tcp 
  ```
  
* En el ambiente de linux puedes ejectuar sin ningun problema como normalmente se hace.

El primero en ejecutarse debe ser el broker y luego los subscriptores y por último los publicadores.

## Subscriptor 
Para ese debes mandar alguno de estos tres mensajes; debende de a que partidos te quieres subscribir, puede ser uno o más y solo una vez por partido: 

Suscribirse al partido Millonarios vs Santa fe 
 ```
   SUB Santa_Fe_vs_Millonarios
  ```

Ssuscribirse al partido Uniandes vs Javeriana
 ```
     SUB Uniandes_vs_Javeriana
  ```

Suscribirse al partido Colombia vs Francia 
 ```
   SUB Colombia_vs_Francia
  ```

Ya con eso le llegaran los mensaje sde dichos partidos 

## Publicador 

Este les pedirá un nombre de un archivo puedes ejegir entre "publicacionoes.txt" o "publicacionesASCUN.txt" 

Estos documentos deben estar en la MISMA carpeta de donde esten los ejecutables; por ahora se encuentran en el bin.
Luego se publicaran los contenidos de dichos txt.  Puede verificar en los subscriptores correspondientes.
Le saldra la pregunta de si quiere publicar más cosas si es asi escriba 's' de lo contrario 'n' 



