/**
 * selector.c - un muliplexor de entrada salida
 */
#include <stdio.h>  // perror
#include <stdlib.h> // malloc
#include <string.h> // memset
#include <assert.h> // :)
#include <errno.h>  // :)
#include <pthread.h>
#include <sys/time.h>

#include <stdint.h> // SIZE_MAX
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/signal.h>
#include "include/selector.h"

#define N(x) (sizeof(x) / sizeof((x)[0]))

#define ERROR_DEFAULT_MSG "something failed"


int ignored_fds[FD_SETSIZE] = {0}; 


static int ignored_fd(int fd) {
    if (fd < 0 || fd >= FD_SETSIZE) {
        return 0; 
    }
    return ignored_fds[fd] == 1;
}

/** retorna una descripción humana del fallo */
const char *
selector_error(const selector_status status)
{
    const char *msg;
    switch (status)
    {
    case SELECTOR_SUCCESS:
        msg = "Success";
        break;
    case SELECTOR_ENOMEM:
        msg = "Not enough memory";
        break;
    case SELECTOR_MAXFD:
        msg = "Can't handle any more file descriptors";
        break;
    case SELECTOR_IARGS:
        msg = "Illegal argument";
        break;
    case SELECTOR_IO:
        msg = "I/O error";
        break;
    default:
        msg = ERROR_DEFAULT_MSG;
    }
    return msg;
}

unsigned int connect_timeout_microsec = DEFAULT_CONNECT_TIMEOUT_MICROSEC;

static void
wake_handler(const int /*signal*/)

{
    // nada que hacer. está solo para interrumpir el select
}

// señal a usar para las notificaciones de resolución
struct selector_init conf;
static sigset_t emptyset, blockset;

selector_status
selector_init(const struct selector_init *c)
{
    memcpy(&conf, c, sizeof(conf));
    // inicializamos el sistema de comunicación entre threads y el selector
    // principal. La técnica se encuentra descripta en
    // "The new pselect() system call" <https://lwn.net/Articles/176911/>
    //  March 24, 2006
    selector_status ret = SELECTOR_SUCCESS;
    struct sigaction act = {
        .sa_handler = wake_handler,
    };

    // 0. calculamos mascara para evitar que se interrumpa antes de llegar al
    //    select
    sigemptyset(&blockset);
    sigaddset(&blockset, conf.signal);
    if (-1 == sigprocmask(SIG_BLOCK, &blockset, NULL))
    {
        ret = SELECTOR_IO;
        goto finally;
    }

    // 1. Registramos una función que atenderá la señal de interrupción
    //    del selector.
    //    Esta interrupción es útil en entornos multi-hilos.

    if (sigaction(conf.signal, &act, 0))
    {
        ret = SELECTOR_IO;
        goto finally;
    }
    sigemptyset(&emptyset);

finally:
    return ret;
}

selector_status
selector_close(void)
{
    // Nada para liberar.
    // TODO(juan): podriamos reestablecer el handler de la señal.
    return SELECTOR_SUCCESS;
}

// estructuras internas
struct item
{
    int fd;
    fd_interest interest;
    const fd_handler *handler;
    void *data;
    struct timeval tv;
    int to_be_removed; 
};

/* tarea bloqueante */
struct blocking_job
{
    /** selector dueño de la resolucion */
    fd_selector s;
    /** file descriptor dueño de la resolucion */
    int fd;

    /** datos del trabajo provisto por el usuario */
    void *data;

    /** el siguiente en la lista */
    struct blocking_job *next;
};

/** marca para usar en item->fd para saber que no está en uso */
static const int FD_UNUSED = -1;

/** verifica si el item está usado */
#define ITEM_USED(i) ((FD_UNUSED != (i)->fd))

struct fdselector
{
    // almacenamos en una jump table donde la entrada es el file descriptor.
    // Asumimos que el espacio de file descriptors no va a ser esparso; pero
    // esto podría mejorarse utilizando otra estructura de datos
    struct item *fds;
    size_t fd_size; // cantidad de elementos posibles de fds

    /** fd maximo para usar en select() */
    int max_fd; // max(.fds[].fd)

    /** descriptores prototipicos ser usados en select */
    fd_set master_r, master_w;
    /** para ser usado en el select() (recordar que select cambia el valor) */
    fd_set slave_r, slave_w;

    /** timeout prototipico para usar en select() */
    struct timespec master_t;
    /** tambien select() puede cambiar el valor */
    struct timespec slave_t;

    // notificaciónes entre blocking jobs y el selector
    volatile pthread_t selector_thread;
    /** protege el acceso a resolutions jobs */
    pthread_mutex_t resolution_mutex;
    /**
     * lista de trabajos blockeantes que finalizaron y que pueden ser
     * notificados.
     */
    struct blocking_job *resolution_jobs;
};

typedef struct fdselector fd_selectorCDT;

/** cantidad máxima de file descriptors que la plataforma puede manejar */
#define ITEMS_MAX_SIZE FD_SETSIZE

// en esta implementación el máximo está dado por el límite natural de select(2).

/**
 * determina el tamaño a crecer, generando algo de slack para no tener
 * que realocar constantemente.
 */
static size_t next_capacity(const size_t n)
{
    unsigned bits = 0;
    size_t tmp = n;
    while (tmp != 0)
    {
        tmp >>= 1;
        bits++;
    }
    tmp = 1UL << bits;

    assert(tmp >= n);
    if (tmp > ITEMS_MAX_SIZE)
    {
        tmp = ITEMS_MAX_SIZE;
    }

    return tmp + 1;
}

static inline void
item_init(struct item *item)
{
    item->fd = FD_UNUSED;
    gettimeofday(&item->tv, NULL);
}

/**
 * inicializa los nuevos items. `last' es el indice anterior.
 * asume que ya está blanqueada la memoria.
 */
static void
items_init(fd_selector s, const size_t last)
{
    assert(last <= s->fd_size);
    for (size_t i = last; i < s->fd_size; i++)
    {
        item_init(s->fds + i);
    }
}

/**
 * calcula el fd maximo para ser utilizado en select()
 */
static int
items_max_fd(fd_selector s)
{
    int max = 0;
    for (int i = 0; i <= s->max_fd; i++)
    {
        struct item *item = s->fds + i;
        if (ITEM_USED(item))
        {
            if (item->fd > max)
            {
                max = item->fd;
            }
        }
    }
    return max;
}

static void
items_update_fdset_for_fd(fd_selector s, struct item *item)
{
    FD_CLR(item->fd, &s->master_r);
    FD_CLR(item->fd, &s->master_w);

    if (ITEM_USED(item))
    {
        if (item->interest & OP_READ)
        {
            FD_SET(item->fd, &(s->master_r));
            gettimeofday(&item->tv, NULL);
        }

        if (item->interest & OP_WRITE)
        {
            FD_SET(item->fd, &(s->master_w));
            gettimeofday(&item->tv, NULL);
        }
    }
}

/**
 * garantizar cierta cantidad de elemenos en `fds'.
 * Se asegura de que `n' sea un número que la plataforma donde corremos lo
 * soporta
 */
static selector_status
ensure_capacity(fd_selector s, const size_t n)
{
    selector_status ret = SELECTOR_SUCCESS;

    const size_t element_size = sizeof(*s->fds);
    if (n < s->fd_size)
    {
        // nada para hacer, entra...
        ret = SELECTOR_SUCCESS;
    }
    else if (n > ITEMS_MAX_SIZE)
    {
        // me estás pidiendo más de lo que se puede.
        ret = SELECTOR_MAXFD;
    }
    else if (NULL == s->fds)
    {
        // primera vez.. alocamos
        const size_t new_size = next_capacity(n);

        s->fds = calloc(new_size, element_size);
        if (NULL == s->fds)
        {
            ret = SELECTOR_ENOMEM;
        }
        else
        {
            s->fd_size = new_size;
            items_init(s, 0);
        }
    }
    else
    {
        // hay que agrandar...
        const size_t new_size = next_capacity(n);
        if (new_size > SIZE_MAX / element_size)
        { // ver MEM07-C
            ret = SELECTOR_ENOMEM;
        }
        else
        {
            struct item *tmp = realloc(s->fds, new_size * element_size);
            if (NULL == tmp)
            {
                ret = SELECTOR_ENOMEM;
            }
            else
            {
                s->fds = tmp;
                const size_t old_size = s->fd_size;
                s->fd_size = new_size;

                items_init(s, old_size);
            }
        }
    }

    return ret;
}

fd_selector
selector_new(const size_t initial_elements)
{
    size_t size = sizeof(struct fdselector);
    fd_selector ret = malloc(size);
    if (ret != NULL)
    {
        memset(ret, 0x00, size);
        ret->master_t.tv_sec = conf.select_timeout.tv_sec;
        ret->master_t.tv_nsec = conf.select_timeout.tv_nsec;
        assert(ret->max_fd == 0);
        ret->resolution_jobs = 0;
        pthread_mutex_init(&ret->resolution_mutex, 0);
        if (0 != ensure_capacity(ret, initial_elements))
        {
            selector_destroy(ret);
            ret = NULL;
        }
    }
    return ret;
}

void selector_destroy(fd_selector s)
{
    // lean ya que se llama desde los casos fallidos de _new.
    if (s != NULL)
    {
        if (s->fds != NULL)
        {
            for (size_t i = 0; i < s->fd_size; i++)
            {
                if (ITEM_USED(s->fds + i))
                {
                    selector_unregister_fd(s, i);
                }
            }
            pthread_mutex_destroy(&s->resolution_mutex);
            struct blocking_job *j = s->resolution_jobs;
            while (j != NULL)
            {
                struct blocking_job *aux = j;
                j = j->next;
                free(aux);
            }
            free(s->fds);
            s->fds = NULL;
            s->fd_size = 0;
        }
        free(s);
    }
}

#define INVALID_FD(fd) ((fd) < 0 || (fd) >= ITEMS_MAX_SIZE)

selector_status
selector_register(fd_selector s,
                  const int fd,
                  const fd_handler *handler,
                  const fd_interest interest,
                  void *data)
{
    selector_status ret = SELECTOR_SUCCESS;
    // 0. validación de argumentos
    if (s == NULL || INVALID_FD(fd) || handler == NULL)
    {
        ret = SELECTOR_IARGS;
        goto finally;
    }
    // 1. tenemos espacio?
    size_t ufd = (size_t)fd;
    if (ufd > s->fd_size)
    {
        ret = ensure_capacity(s, ufd);
        if (SELECTOR_SUCCESS != ret)
        {
            goto finally;
        }
    }

    // 2. registración
    struct item *item = s->fds + ufd;
    if (ITEM_USED(item))
    {
        ret = SELECTOR_FDINUSE;
        goto finally;
    }
    else
    {
        item->fd = fd;
        item->handler = handler;
        item->interest = interest;
        item->data = data;

        // actualizo colaterales
        if (fd > s->max_fd)
        {
            s->max_fd = fd;
        }
        items_update_fdset_for_fd(s, item);
    }

finally:
    return ret;
}

selector_status
selector_unregister_fd(fd_selector s,
                       const int fd)
{
    selector_status ret = SELECTOR_SUCCESS;

    if (NULL == s || INVALID_FD(fd))
    {
        ret = SELECTOR_IARGS;
        goto finally;
    }

    struct item *item = s->fds + fd;
    if (!ITEM_USED(item))
    {
        ret = SELECTOR_IARGS;
        goto finally;
    }

    if (item->handler->handle_close != NULL)
    {
        struct selector_key key = {
            .s = s,
            .fd = item->fd,
            .data = item->data,
        };
        item->handler->handle_close(&key);
    }

    item->interest = OP_NOOP;
    items_update_fdset_for_fd(s, item);

    memset(item, 0x00, sizeof(*item));
    item_init(item);
    s->max_fd = items_max_fd(s);

finally:
    return ret;
}

selector_status
selector_set_interest(fd_selector s, int fd, fd_interest i)
{
    selector_status ret = SELECTOR_SUCCESS;

    if (NULL == s || INVALID_FD(fd))
    {
        ret = SELECTOR_IARGS;
        goto finally;
    }
    struct item *item = s->fds + fd;
    if (!ITEM_USED(item))
    {
        ret = SELECTOR_IARGS;
        goto finally;
    }
    item->interest = i;
    items_update_fdset_for_fd(s, item);
finally:
    return ret;
}

selector_status
selector_set_interest_key(struct selector_key *key, fd_interest i)
{
    selector_status ret;

    if (NULL == key || NULL == key->s || INVALID_FD(key->fd))
    {
        ret = SELECTOR_IARGS;
    }
    else
    {
        ret = selector_set_interest(key->s, key->fd, i);
    }

    return ret;
}

/**
 * se encarga de manejar los resultados del select.
 * se encuentra separado para facilitar el testing
 */
static void
handle_iteration(fd_selector s)
{
    int n = s->max_fd;
    struct selector_key key = {
        .s = s,
    };

    for (int i = 0; i <= n; i++)
    {
        struct item *item = s->fds + i;
        if (ITEM_USED(item))
        {
            key.fd = item->fd;
            key.data = item->data;
            if (FD_ISSET(item->fd, &s->slave_r))
            {
                if (OP_READ & item->interest)
                {
                    if (0 == item->handler->handle_read)
                    {
                        assert(("OP_READ arrived but no handler. bug!" == 0));
                    }
                    else
                    {
                        item->handler->handle_read(&key);
                        gettimeofday(&item->tv, NULL);
                    }
                }
            }
            if (FD_ISSET(i, &s->slave_w))
            {
                if (OP_WRITE & item->interest)
                {
                    if (0 == item->handler->handle_write)
                    {
                        assert(("OP_WRITE arrived but no handler. bug!" == 0));
                    }
                    else
                    {
                        item->handler->handle_write(&key);
                        gettimeofday(&item->tv, NULL);
                    }
                }
            }
        }
    }
}

static void
handle_block_notifications(fd_selector s)
{
    struct selector_key key = {
        .s = s,
    };
    pthread_mutex_lock(&s->resolution_mutex);
    struct blocking_job *j = s->resolution_jobs;
    while (j != NULL)
    {

        struct item *item = s->fds + j->fd;
        if (ITEM_USED(item))
        {
            key.fd = item->fd;
            key.data = item->data;
            item->handler->handle_block(&key);
        }

        struct blocking_job *aux = j;
        j = j->next;
        free(aux);
    }
    s->resolution_jobs = 0;
    pthread_mutex_unlock(&s->resolution_mutex);
}

selector_status
selector_notify_block(fd_selector s,
                      const int fd)
{
    selector_status ret = SELECTOR_SUCCESS;

    // TODO(juan): usar un pool
    struct blocking_job *job = malloc(sizeof(*job));
    if (job == NULL)
    {
        ret = SELECTOR_ENOMEM;
        goto finally;
    }
    job->s = s;
    job->fd = fd;

    // encolamos en el selector los resultados
    pthread_mutex_lock(&s->resolution_mutex);
    job->next = s->resolution_jobs;
    s->resolution_jobs = job;
    pthread_mutex_unlock(&s->resolution_mutex);

    // notificamos al hilo principal
    pthread_kill(s->selector_thread, conf.signal);

finally:
    return ret;
}

selector_status
selector_select(fd_selector s)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);


    for (int i = 0; i <= s->max_fd; i++)
    {
        struct item *item = s->fds + i;
        if (item->to_be_removed) {
            selector_unregister_fd(s, i);
            close(i);
            item->to_be_removed = 0;
        }
        if (ITEM_USED(item))
        {
            //printf("timeout: %ul", conf.connect_timeout_microsec);
            if (!ignored_fd(item->fd)&&(tv.tv_sec - item->tv.tv_sec) * 1000000 + (tv.tv_usec - item->tv.tv_usec) > connect_timeout_microsec)
            {
                //printf("FD %d timed out with time diff being %ld microseconds\n", item->fd, (tv.tv_sec - item->tv.tv_sec) * 1000000 + (tv.tv_usec - item->tv.tv_usec));
                selector_unregister_fd(s, i);
                close(i);
            } 
        }
    }

    selector_status ret = SELECTOR_SUCCESS;

    memcpy(&s->slave_r, &s->master_r, sizeof(s->slave_r));
    memcpy(&s->slave_w, &s->master_w, sizeof(s->slave_w));
    
    
    memcpy(&s->slave_t, &s->master_t, sizeof(s->slave_t));
    s->slave_t.tv_sec = connect_timeout_microsec / 1000000;


    s->selector_thread = pthread_self();

    int fds = pselect(s->max_fd + 1, &s->slave_r, &s->slave_w, 0, &s->slave_t,
                      &emptyset);
    if (-1 == fds)
    {
        switch (errno)
        {
        case EAGAIN:
        case EINTR:
            // si una señal nos interrumpio. ok!
            break;
        case EBADF:
            // ayuda a encontrar casos donde se cierran los fd pero no
            // se desregistraron
            for (int i = 0; i < s->max_fd; i++)
            {
                if (FD_ISSET(i, &s->master_r) || FD_ISSET(i, &s->master_w))
                {
                    if (-1 == fcntl(i, F_GETFD, 0))
                    {
                        fprintf(stderr, "Bad descriptor detected: %d\n", i);
                    }
                }
            }
            ret = SELECTOR_IO;
            break;
        default:
            ret = SELECTOR_IO;
            goto finally;
        }
    }
    else
    {
        handle_iteration(s);
    }
    if (ret == SELECTOR_SUCCESS)
    {
        handle_block_notifications(s);
    }
finally:
    return ret;
}

int selector_fd_set_nio(const int fd)
{
    int ret = 0;
    int flags = fcntl(fd, F_GETFD, 0);
    if (flags == -1)
    {
        ret = -1;
    }
    else
    {
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        {
            ret = -1;
        }
    }
    return ret;
}

void update_connect_timeout(unsigned int timeout_microsec){
    connect_timeout_microsec = timeout_microsec;
    printf("UPDATE CONNECT TIMEOUT: %u microseconds\n", connect_timeout_microsec);
}

void add_ignored_fd(int fd) {
    if (fd < 0 || fd >= FD_SETSIZE) {
        return; 
    }
    if (ignored_fds[fd] == 0) {
        ignored_fds[fd] = 1; 
    }
}

void remove_ignored_fd(int fd, void * fds) {
    if (fd < 0 || fd >= FD_SETSIZE) {
        return; // Invalid fd, do nothing
    }
    if (ignored_fds[fd] == 1) {
        ignored_fds[fd] = 0; 
    }
    fd_selector selector = (fd_selector)fds;
    selector->fds[fd].to_be_removed=1;
}
