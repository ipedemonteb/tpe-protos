#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>

#include "include/args.h"

static char *
port(const char* s)
{
    char* end = 0;
    const long sl = strtol(s, &end, 10);

    if (end == s || '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 1 || sl > USHRT_MAX)
    {
        fprintf(stderr, "port should be in the range of 1-65535: %s\n", s);
        exit(1);
        return NULL;
    }
    return (char*)s;
}

static void
user(char* s, struct users* user)
{
    char* p = strchr(s, ':');
    if (p == NULL)
    {
        fprintf(stderr, "password not found\n");
        exit(1);
    }
    else
    {
        *p = 0;
        p++;
        user->name = s;
        user->pass = p;
    }
}

static void
version(void)
{
    fprintf(stderr, "socks5v version 1.0\n"
        "ITBA Protocolos de Comunicación 2025/1 -- Grupo 3\n"
        "Licensed under the Apache License, Version 2.0\n"
        "You may obtain a copy of the License at\n"
        "\n"
        "  http://www.apache.org/licenses/LICENSE-2.0\n"
        );
}

static void
usage(const char* progname)
{
    fprintf(stderr,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h               Imprime la ayuda y termina.\n"
            "   -l <SOCKS addr>  Dirección donde servirá el proxy SOCKS.\n"
            "   -L <conf  addr>  Dirección donde servirá el servicio de management.\n"
            "   -p <SOCKS port>  Puerto entrante conexiones SOCKS.\n"
            "   -P <conf port>   Puerto entrante conexiones configuracion\n"
            "   -u <name>:<pass> Usuario y contraseña de usuario que puede usar el proxy. Hasta 10.\n"
            "   -v               Imprime información sobre la versión versión y termina.\n"

            "\n",
            progname);
    exit(1);
}

static void
usage_monitor(const char* progname)
{
    fprintf(stderr,
            "Usage: %s [OPTION]...\n"
            "\n"
            "   -h               Imprime la ayuda y termina.\n"
            "   -L <conf addr>   Dirección del servicio de management.\n"
            "   -P <conf port>   Puerto del servicio de management.\n"
            "\n",
            progname);
    exit(1);
}

void
parse_args(const int argc, char** argv, struct socks5args* args)
{
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users

    args->socks_addr = "0.0.0.0";
    args->socks_port = "1080";

    args->mng_addr = "127.0.0.1";
    args->mng_port = "8080";

    args->disectors_enabled = true;

    int c;
    args->nusers = 0;

    while (true)
    {
        int option_index = 0;
        static struct option long_options[] = {
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "hl:L:Np:P:u:v", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
            usage(argv[0]);
            break;
        case 'l':
            args->socks_addr = optarg;
            break;
        case 'L':
            args->mng_addr = optarg;
            break;
        case 'N':
            args->disectors_enabled = false;
            break;
        case 'p':
            args->socks_port = port(optarg);
            break;
        case 'P':
            args->mng_port = port(optarg);
            break;
        case 'u':
            if (args->nusers >= MAX_USERS)
            {
                fprintf(stderr, "maximun number of command line users reached: %d.\n", MAX_USERS);
                exit(1);
            }
            else
            {
                user(optarg, args->users + args->nusers);
                args->nusers++;
            }
            break;
        case 'v':
            version();
            exit(0);
        default:
            fprintf(stderr, "unknown argument %d.\n", c);
            exit(1);
        }
    }
    if (optind < argc)
    {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc)
        {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    }
}

void
parse_monitor_args(const int argc, char** argv, struct monitor_args* args)
{
    memset(args, 0, sizeof(*args));

    args->addr = "127.0.0.1";
    args->port = "8080";

    int c;
    while (true)
    {
        int option_index = 0;
        static struct option long_options[] = {
            {0, 0, 0, 0}
        };

        c = getopt_long(argc, argv, "hL:P:", long_options, &option_index);
        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
            usage_monitor(argv[0]);
            break;
        case 'L':
            args->addr = optarg;
            break;
        case 'P':
            args->port = port(optarg);
            break;
        default:
            fprintf(stderr, "unknown argument %d.\n", c);
            exit(1);
        }
    }
    if (optind < argc)
    {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc)
        {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    }
}
