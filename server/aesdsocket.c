#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/queue.h>
#include <time.h>
#include <pthread.h>

#define PORT            9000
#define BACKLOG         1
#define DATA_FILE       "/var/tmp/aesdsocketdata"
#define BUF_SIZE        1024

static int server_fd = -1;
static volatile sig_atomic_t do_exit = 0;
static pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

TAILQ_HEAD(thread_list, client_thread);
struct thread_list thread_head = TAILQ_HEAD_INITIALIZER(thread_head);

typedef struct client_thread {
    pthread_t tid;
    TAILQ_ENTRY(client_thread) entries;
} client_thread_t;

static void signal_handler(int signum)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    do_exit = 1;
}

static void cleanup(void)
{
    if (server_fd >= 0) {
        close(server_fd);
    }
    unlink(DATA_FILE);
    closelog();
}

static void timer_handler(union sigval sv)
{
    (void)sv;
    char tsbuf[64];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(tsbuf, sizeof(tsbuf),"%a, %d %b %Y %H:%M:%S %z", &tm);

    pthread_mutex_lock(&file_mutex);
    FILE *f = fopen(DATA_FILE, "a+");
    if (f) {
        fprintf(f, "timestamp:%s\n", tsbuf);
        fclose(f);
    }
    pthread_mutex_unlock(&file_mutex);
}

static void *connection_handler(void *_pconn_fd)
{
    int conn_fd = *(int*)_pconn_fd;
    free(_pconn_fd);

    struct sockaddr_in peer;
    socklen_t plen = sizeof(peer);
    getpeername(conn_fd, (struct sockaddr*)&peer, &plen);
    char *ip = inet_ntoa(peer.sin_addr);
    syslog(LOG_INFO, "Accepted connection from %s", ip);

    char buf[BUF_SIZE];
    size_t buf_len = 0;

    while (!do_exit) {
        ssize_t r = read(conn_fd, buf + buf_len, BUF_SIZE - buf_len);
        if (r <= 0) break;

        buf_len += r;
        bool found_newline = false;

        size_t processed = 0;
while (true) {
    void *newline = memchr(buf + processed, '\n', buf_len - processed);
    if (!newline) break;

    size_t write_len = (char*)newline - (buf + processed) + 1;

    pthread_mutex_lock(&file_mutex);
    FILE *df = fopen(DATA_FILE, "a");
    if (df) {
        fwrite(buf + processed, 1, write_len, df);
        fclose(df);
    }
    pthread_mutex_unlock(&file_mutex);

    pthread_mutex_lock(&file_mutex);
    df = fopen(DATA_FILE, "r");
    if (df) {
        int c;
        while ((c = fgetc(df)) != EOF) {
            if (write(conn_fd, &c, 1) < 0) break;
        }
        fclose(df);
    }
    pthread_mutex_unlock(&file_mutex);

    processed += write_len;
}

if (processed > 0 && processed < buf_len) {
    memmove(buf, buf + processed, buf_len - processed);
}
buf_len -= processed;

    }

    syslog(LOG_INFO, "Closed connection from %s", ip);
    close(conn_fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    bool daemon_mode = false;
    struct sigaction sa = {0};
    struct sockaddr_in addr = {0};
    int opt = 1;

    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    } else if (argc > 1) {
        fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    unlink(DATA_FILE);

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    atexit(cleanup);

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR | SO_REUSEPORT,
                   &opt,
                   sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (daemon_mode) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            exit(EXIT_SUCCESS);
        }
        if (setsid() < 0) {
            perror("setsid");
        }
        if (chdir("/") < 0) {
            perror("chdir");
        }
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2) close(fd);
        }
    }

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    {
        struct sigevent sev = {
            .sigev_notify          = SIGEV_THREAD,
            .sigev_notify_function = timer_handler,
        };
        timer_t tid;
        timer_create(CLOCK_REALTIME, &sev, &tid);
        struct itimerspec its = {
            .it_value    = {10,0},
            .it_interval = {10,0},
        };
        timer_settime(tid, 0, &its, NULL);
    }

    while (!do_exit) {
        int *pconn_fd = malloc(sizeof(*pconn_fd));
        *pconn_fd = accept(server_fd, NULL, NULL);
        if (*pconn_fd < 0) {
            free(pconn_fd);
            if (errno == EINTR && do_exit) break;
            perror("accept");
            continue;
        }

        client_thread_t *t = calloc(1, sizeof(*t));
        pthread_create(&t->tid, NULL, connection_handler, pconn_fd);
        TAILQ_INSERT_TAIL(&thread_head, t, entries);
    }

    struct client_thread *t;
    while (!TAILQ_EMPTY(&thread_head)) {
        t = TAILQ_FIRST(&thread_head);
        pthread_join(t->tid, NULL);
        TAILQ_REMOVE(&thread_head, t, entries);
        free(t);
    }

    return 0;
}
