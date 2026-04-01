# Lab 3: Sockets TCP y UDP

Este proyecto consiste en una plataforma de distribución de noticias de fútbol en tiempo real. Utiliza un modelo de Publicación-Suscripción donde un nodo central (Broker) gestiona el flujo de información entre periodistas (Publishers) y aficionados (Subscribers). Tiene dos versiones TCP y UDP para hacer pruebas de rendimiento.

## 1. Integrantes
- Laura Avelino
- Alejandro Franco
- Kevin Hernandez 
- Laura Martinez
 
## 2. Sockets UDP

# Compilar los archivos y Ejecutar en Terminales (UDP):

#### Genera los ejecutables desde tu directorio de trabajo:

gcc broker_udp.c -o broker
gcc publisher_udp.c -o publisher
gcc subscriber_udp.c -o subscriber

TODAS LAS TERMINALES DEBEN ESTAR SOBRE EL DIRECTORIO DE TRABAJO DONDE ESTAN LOS EJECUTABLES.

#### Lanzar el Broker (Terminal 1)
Este será el corazón del sistema. Debe estar encendido siempre antes que los demás.
Ejecutar en la Terminal: ./broker 8080

#### Suscribir a los hinchas (Terminales 2 y 3)
Imagina que hay dos personas viendo partidos distintos.

En la Terminal 2 (Seguir a Colombia):
Ejecutar en la Terminal: ./subscriber 127.0.0.1 8080 Colombia_vs_Brasil

En la Terminal 3 (Seguir a Argentina):
Ejecutar en la Terminal: ./subscriber 127.0.0.1 8080 Argentina_vs_Chile

#### Publicar las noticias (Terminales 4 y 5)
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
Aqui ira todo lo de TCP y la explicacion de la implementación 
