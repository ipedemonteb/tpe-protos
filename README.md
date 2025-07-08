# TPE - Protocolos de Comunicación

El presente trabajo consiste en el desarrollo de un servidor proxy SOCKSv5 con capacidades de monitoreo y configuración en tiempo real, entre las cuales se encuentran:

- **Servidor SOCKSv5**: Implementación completa del protocolo SOCKSv5 con autenticación
- **Protocolo de Monitoreo**: Protocolo de monitoreo y configuración en tiempo real
- **Cliente de Monitoreo**: Aplicación cliente para interactuar con el servidor de monitoreo
- **Sistema de Métricas**: Tracking de conexiones, bytes transferidos y usuarios activos

<details>
  <summary>Contenidos</summary>
  <ol>
    <li><a href="#instalación">Instalación</a></li>
    <li><a href="#instrucciones">Instrucciones</a></li>
    <li><a href="#protocolo-de-monitoreo">Protocolo de Monitoreo</a></li>
    <li><a href="#arquitectura">Arquitectura</a></li>
    <li><a href="#estructura-del-proyecto">Estructura del Proyecto</a></li>
    <li><a href="#integrantes">Integrantes</a></li>
  </ol>
</details>

## Instalación:

Clonar el repositorio:

- HTTPS:
  ```sh
  git clone XXX
  ```
- SSH:
  ```sh
  git clone XXX
  ```

Luego, compilar el proyecto:

```sh
make
```

Esto generará dos ejecutables:
- `server`: Servidor principal (SOCKSv5 + Monitoreo)
- `monitor_client`: Cliente de monitoreo

<p align="right">(<a href="#tpe---protocolos-de-comunicación">Volver</a>)</p>

## Instrucciones:

Una vez ejecutado el código 

### Feature 1:

Para el manejo de memoria, se puede ejecutar el comando ```mem``` 

### Feature 2:

Los procesos...

### Servidor Principal:
```sh
./server [puerto_socks] [puerto_monitor]
```

Por defecto:
- Puerto SOCKSv5: 1080
- Puerto Monitoreo: 9090

Ejemplo:
```sh
./server 1080 9090
```

### Cliente de Monitoreo:
```sh
./monitor_client [host] [puerto]
```

Por defecto se conecta a localhost:9090.

### Comandos del Cliente de Monitoreo:

- `STATS`: Obtiene estadísticas generales del servidor
- `CONNECTIONS`: Obtiene información sobre conexiones actuales
- `USERS`: Lista usuarios activos
- `CONFIG <param> <value>`: Cambia configuración en tiempo real
  - `CONFIG timeout 30`: Cambia el timeout a 30 segundos
  - `CONFIG max_connections 1000`: Cambia el máximo de conexiones a 1000
- `quit`: Sale del cliente

<p align="right">(<a href="#tpe---protocolos-de-comunicación">Volver</a>)</p>

## Protocolo de Monitoreo

### Handshake:
```
S: SOCKS-MONITOR 1.0
C: MONITOR-CLIENT 1.0
```

<p align="right">(<a href="#tpe---protocolos-de-comunicación">Volver</a>)</p>

## Integrantes:

Celestino Garrós (64375) - cgarros@itba.edu.ar

Ignacio Pedemonte Berthoud (64908) - ipedemonteberthoud@itba.edu.ar

Federico Ignacio Ruckauf (64356) - fruckauf@itba.edu.ar

Leo Weitz (64365) - lweitz@itba.edu.ar


<p align="right">(<a href="#tpe---protocolos-de-comunicación">Volver</a>)</p>