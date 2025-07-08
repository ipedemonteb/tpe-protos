## Protocolo de Monitoreo

### Comandos del Cliente

- `STATS`: Obtiene estadísticas generales del servidor.
- `CONNECTIONS`: Obtiene información sobre conexiones actuales.
- `USERS`: Lista usuarios activos.
- `CONFIG <param> <value>`: Cambia configuración en tiempo real.
    - Ejemplo: `CONFIG timeout 30` (cambia el timeout a 30 segundos)
    - Ejemplo: `CONFIG max_connections 1000` (cambia el máximo de conexiones a 1000)
- `quit`: Sale del cliente.


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
- Máximo de conexiones concurrentes.

### Flow

#### Handshake

- S: [BANNER] [ESPACIO] [VERSION] [\n]
- C: [BANNER] [ESPACIO] [VERSION] [\n]
- S: [PROVIDE_CREDENTIALS_BANNER] [\n]
- C: [USERNAME]/[PASSWORD] [\n]
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
- `QUIT`
    - S: Cierra la conexión.

#### Ejemplo de comunicación

```
S: SOCKS-MONITOR 1.0
C: SoyUnCliente 1.0
S: Please provide your credentials
C: elUsernombre/laContraseña
S: The credentials are valid. You can proceed with the monitoring!
C: USERS
S: OK No active users
C: STATS
S: OK connections:0 bytes:0 users:0 concurrent:0 max_concurrent:0 uptime:52
C: STAST
S: ERR Unknown command: STAST
C: STATS
S: OK connections:1 bytes:0 users:1 concurrent:1 max_concurrent:1 uptime:85
C: CONNECTIONS
S: OK current:1 max:1 allowed:500
C: CONFIG timeout 1
S: OK timeout set to 1
C: QUIT
[S: Conexión cerrada]
```
