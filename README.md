# TPE - Protocolos de Comunicación

El presente trabajo consiste en el desarrollo de un servidor proxy SOCKSv5 con capacidades de monitoreo y configuración en tiempo real, entre las cuales se encuentran:

- **Servidor SOCKSv5**: Implementación completa del protocolo SOCKSv5 con autenticación.
- **Protocolo de Monitoreo**: Protocolo de monitoreo y configuración en tiempo real.
- **Cliente de Monitoreo**: Aplicación cliente para interactuar con el servidor de monitoreo.
- **Sistema de Métricas**: Tracking de conexiones, bytes transferidos y usuarios activos.

<details>
  <summary>Contenidos</summary>
  <ol>
    <li><a href="#instalación">Instalación</a></li>
    <li><a href="#instrucciones">Instrucciones</a></li>
    <li><a href="#limpieza">Limpieza</a></li>
    <li><a href="#estructura">Estructura</a></li>
    <li><a href="#integrantes">Integrantes</a></li>
  </ol>
</details>

## Instalación:

Clonar el repositorio:

- HTTPS:
  ```sh
  git clone https://github.com/ipedemonteb/tpe-protos.git

  ```
- SSH:
  ```sh
  git clone git@github.com:ipedemonteb/tpe-protos.git 
  ```

Luego, compilar el proyecto:

```sh
make
```

Esto generará dos ejecutables en la carpeta `bin`:
- `server`: Servidor principal (SOCKSv5 + Monitoreo).
- `monitor_client`: Cliente de monitoreo.

<p align="right">(<a href="#tpe---protocolos-de-comunicación">Volver</a>)</p>

## Instrucciones:

### Servidor Proxy:

Para ejecutar el servidor, se debe ejecutar el siguiente comando:

```sh
./bin/server -flag <FLAG_PARAM>
```

Ejemplo:
```sh
./bin/server -p 9090
```

El servidor principal puede inicializarse con los siguientes flags:

| **Flag**              | **Descripción** |
|-----------------------|-----------------|
| `-l <SOCKS ADDR>`     | Define la dirección IP en la que el servidor escuchará por conexiones entrantes SOCKS (`0.0.0.0` por defecto). |
| `-p <SOCKS PORT>`     | Especifica el puerto en el que se aceptarán conexiones SOCKS (`1080` por defecto). |
| `-L <CONF ADDR>`      | Dirección IP en la que se expondrá el servicio de monitoreo y administración (`127.0.0.1` por defecto). |
| `-P <CONF PORT>`      | Puerto por el cual se aceptarán conexiones al servicio de monitoreo (`8080` por defecto). |
| `-u <NAME>:<PASS>`    | Agrega un usuario autorizado para usar el proxy SOCKS, junto con su contraseña. Se pueden definir hasta un máximo de 10 usuarios utilizando múltiples instancias de este flag. |
| `-v`                  | Imprime información sobre la versión del servidor y finaliza la ejecución. |


### Cliente de Monitoreo:

Para ejecutar la aplicación cliente de monitoreo, se debe ejecutar el siguiente comando:

```sh
./bin/monitor_client -flag <FLAG_PARAM>
```

Ejemplo:
```sh
./bin/monitor_client -P 9090
```

La aplicación puede utilizarse con los siguientes flags:

| **Flag**              | **Descripción** |
|-----------------------|-----------------|
| `-L <CONF ADDR>`      | Dirección IP del servidor de administración al que se desea conectar. |
| `-P <CONF PORT>`      | Puerto correspondiente al servicio de administración del servidor. |
| `-h`                  | Imprime un mensaje de ayuda con los posibles flags y finaliza la ejecución. |



### Comandos del Cliente de Monitoreo:

Una vez corriendo la aplicación cliente, se cuenta con las siguientes funcionalidades de monitoreo y/o configuración:

- `STATS`: Obtiene estadísticas generales del servidor.
- `CONNECTIONS`: Obtiene información sobre conexiones actuales.
- `USERS`: Lista usuarios activos.
- `CONFIG <param> <value>`: Cambia configuración en tiempo real. Ejemplo:
  - `CONFIG timeout 30`: Cambia el timeout a 30 segundos.
- `ACCESS_LOG <username>`: Si NO se especificó el parámetro username, el servidor retorna la lista de sitios a los que accedió cada usuario a través del servidor. Si se especificó el parámetro username, el servidor retorna la lista de sitios a los que accedió el usuario a través del servidor.
- `QUIT`: Sale del cliente

<p align="right">(<a href="#tpe---protocolos-de-comunicación">Volver</a>)</p>

## Limpieza:

Para hacer una limpieza de los archivos generados en la instalación, se cuenta con el siguiente comando:

```sh
make clean
```

<p align="right">(<a href="#tpe---protocolos-de-comunicación">Volver</a>)</p>

## Estructura:

El proyecto se estructura de la siguiente manera, guardando los archivos objeto dentro del directorio obj, y los archivos ejecutables dentro del directorio bin, como se demostró anteriormente.

```
.
├── Makefile
├── Makefile.inc
├── README.md
├── doc
│   ├── Informe Proyecto Especial - Grupo 3.pdf
│   └── protocol_map.md
├── src
│   ├── client
│   │   └── monitor_client.c
│   ├── server
│   │   ├── include
│   │   │   ├── defines.h
│   │   │   ├── metrics.h
│   │   │   ├── monitor_handler.h
│   │   │   └── socks5_handler.h
│   │   ├── metrics.c
│   │   ├── monitor_handler.c
│   │   ├── server.c
│   │   ├── socks5_handler.c
│   │   ├── states
│   │   │   ├── include
│   │   │   │   ├── state_auth.h
│   │   │   │   ├── state_connect.h
│   │   │   │   ├── state_forward.h
│   │   │   │   ├── state_hello.h
│   │   │   │   ├── state_request.h
│   │   │   │   └── state_utils.h
│   │   │   ├── state_auth.c
│   │   │   ├── state_connect.c
│   │   │   ├── state_forward.c
│   │   │   ├── state_hello.c
│   │   │   ├── state_request.c
│   │   │   └── state_utils.c
│   │   └── utils
│   │       ├── buffer.c
│   │       ├── hashmap.c
│   │       ├── include
│   │       │   ├── buffer.h
│   │       │   ├── hashmap.h
│   │       │   ├── selector.h
│   │       │   ├── server_utils.h
│   │       │   ├── stm.h
│   │       │   └── user_auth_utils.h
│   │       ├── selector.c
│   │       ├── server_utils.c
│   │       ├── stm.c
│   │       └── user_auth_utils.c
│   └── shared
│       ├── args.c
│       ├── include
│       │   ├── args.h
│       │   ├── logger.h
│       │   └── util.h
│       ├── logger.c
│       └── util.c

```


<p align="right">(<a href="#tpe---protocolos-de-comunicación">Volver</a>)</p>

## Integrantes:

Celestino Garrós (64375) - cgarros@itba.edu.ar

Ignacio Pedemonte Berthoud (64908) - ipedemonteberthoud@itba.edu.ar

Federico Ignacio Ruckauf (64356) - fruckauf@itba.edu.ar

Leo Weitz (64365) - lweitz@itba.edu.ar


<p align="right">(<a href="#tpe---protocolos-de-comunicación">Volver</a>)</p>