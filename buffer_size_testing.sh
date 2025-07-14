#!/bin/bash

SERVER_BINARY="server"
SERVER_PORT="1080"
# URL de un archivo de prueba de 100MB
TEST_URL="https://nbg1-speed.hetzner.com/100MB.bin"
RESULTS_FILE="buffer_test_results.csv"

# Usuario y contraseña para la prueba de autenticación SOCKS5
PROXY_USER="legoat"
PROXY_PASS="legches"

BUFFER_SIZES=(1024 2048 4096 8192 16384 32768 65536)

echo "buffer_size_bytes,throughput_mbps" > ${RESULTS_FILE}
echo "----------------------------------------------------"
echo "  Buffer (bytes)  |  Throughput (MB/s)"
echo "----------------------------------------------------"

for size in "${BUFFER_SIZES[@]}"; do
    make clean > /dev/null 2>&1
    
    MAKE_CFLAGS="-Wall -Wextra -g -pthread -D APP_BUFFER_SIZE=${size}"
    make CFLAGS="${MAKE_CFLAGS}"
    
    if [ ! -f "$SERVER_BINARY" ]; then
        echo "Error de compilacion con buffer de ${size} bytes. Saltando..."
        continue
    fi

    # Iniciar nuestro servidor SOCKS5 con un usuario de prueba.
    # AVISO: Ajusta los argumentos si tu servidor usa flags diferentes para los usuarios.
    ./${SERVER_BINARY} -u ${PROXY_USER}:${PROXY_PASS} &
    SERVER_PID=$!
    sleep 1 # Dar tiempo a que el servidor se inicie

    # Usar curl para DESCARGAR el archivo a través del proxy SOCKS5 con autenticación
    # -4: Forzar el uso de IPv4
    # -w '%{speed_download}': Medir la velocidad de descarga
    DOWNLOAD_SPEED_BPS=$(curl -s -o /dev/null \
        -4 \
        --socks5-basic \
        --proxy-user ${PROXY_USER}:${PROXY_PASS} \
        --socks5 localhost:${SERVER_PORT} \
        -w '%{speed_download}' \
        ${TEST_URL})

    # Convertir velocidad de Bytes/seg a MB/s (1024*1024)
    THROUGHPUT=$(echo "${DOWNLOAD_SPEED_BPS}" | awk '{printf "%.2f", $1 / (1024*1024)}')

    if [ -z "$THROUGHPUT" ] || [ "$(echo "$THROUGHPUT < 0.01" | bc -l)" -eq 1 ]; then
        THROUGHPUT="FAIL"
    fi

    # Detener el servidor
    kill ${SERVER_PID} > /dev/null 2>&1
    wait ${SERVER_PID} 2>/dev/null

    printf " %-16s |  %s\n" "$size" "$THROUGHPUT"
    echo "$size,${THROUGHPUT}" >> ${RESULTS_FILE}
done

make clean > /dev/null 2>&1

echo "----------------------------------------------------"
echo "Pruebas completadas. Resultados guardados en ${RESULTS_FILE}"
