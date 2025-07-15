## Protocolo de Monitoreo

### Comandos del Cliente

- `STATS`: Obtiene estadísticas generales del servidor.
- `CONNECTIONS`: Obtiene información sobre conexiones actuales.
- `USERS`: Lista usuarios activos.
- `CONFIG <param> <value>`: Cambia configuración en tiempo real.
    - Ejemplo: `CONFIG timeout 30` (cambia el timeout a 30 segundos)
    - Ejemplo: `CONFIG max_connections 1000` (cambia el máximo de conexiones a 1000)
- `ACCESS_LOG <username>`: Imprime el registro de los sitios a los que accedió cada usuario a traves del servidor. Si se especifica el parametro opcional <username>, se mostraran solo los accesos de ese usuario. Cada linea del registro tiene el formato:
      - [USERNAME]: [DOMAIN]:[PORT] [DATE&TIME] [STATE]
- `QUIT`: Cierra la conexión.


### Decisiones

- Protocolo orientado a conexión.
- Transporte: TCP.
- Separación de líneas con `\n`.
- Comandos y parámetros en texto plano, solo caracteres a-z, A-Z, 0-9, _.
- Los nombres de comandos y parámetros deben tener entre 1 y 255 caracteres inclusive.

### Métricas y Monitoreo

El servidor debe recolectar y exponer métricas útiles para el monitoreo, incluyendo pero no limitado a:

- Cantidad de conexiones históricas.
- Cantidad de conexiones concurrentes.
- Cantidad de bytes transferidos.
- Otras métricas relevantes para entender el funcionamiento dinámico del sistema.

Las métricas pueden ser volátiles (se pierden si el servidor se reinicia).

### Configuración Dinámica

El servidor debe permitir modificar parámetros de configuración en tiempo real mediante el comando `CONFIG`. Ejemplos de parámetros configurables:

- Timeout de conexiones.

### Flow

#### Handshake

- S: [PROVIDE_CREDENTIALS_BANNER] [\n]
- C: [USERNAME]:[PASSWORD] [\n]
- S: [VALID/INVALID_BANNER] [\n]

#### Operación

Cada comando es una línea enviada por el cliente. El servidor responde según el comando:

- `STATS`
    - S: Responde con estadísticas generales (ejemplo: conexiones históricas, bytes transferidos, etc.)
- `CONNECTIONS`
    - S: Lista de conexiones actuales y su estado.
- `USERS`
    - S: Lista de usuarios activos.
- `CONFIG <param> <value>`
    - S: OK si el cambio fue exitoso, ERR si hubo un error.
- `ACCESS_LOG <username>`:
  - Si NO se especificó el parametro username
    - S: OK Access log for all users:
    - S: Lista de sitios a los que accedió cada usuario a traves del servidor.
  - Si se especificó el parametro username
    - S: OK Access log for user a:
    - S: Lista de sitios a los que accedió el usuario a traves del servidor.
- `QUIT`
    - S: Cierra la conexión.

#### Ejemplo de comunicación

```
S> Please enter Username:password (in that format)
C> iamauser:thisismypass
S> OK Welcome iamauser
S> Type commands and press Enter. Type 'quit' to exit.

C> STATS
S> OK connections:3 bytes:111230 users:2 concurrent:0 max_concurrent:1 uptime:633
C> USERS
S> OK a username
C> CONNECTIONS
S> OK current:0 max:1
C> ACCESS_LOG
S> OK Access log for all users:
   a: www.google.com:80 [2025-07-15 01:29:41] SUCCESS
   a: www.google.com:80 [2025-07-15 01:31:15] SUCCESS
   username: www.google.com:80 [2025-07-15 01:31:47] SUCCESS
C> ACCESS_LOG a
S> OK Access log for user a:
   www.google.com:80 [2025-07-15 01:29:41] SUCCESS
   www.google.com:80 [2025-07-15 01:31:15] SUCCESS
C> CONFIG timeout 15
S> OK timeout set to 15
C> QUIT
[S: Conexión cerrada]
```
