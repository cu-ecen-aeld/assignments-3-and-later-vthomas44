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
#include <sys/ioctl.h>
#include <linux/fs.h>
#include "../aesd-char-driver/aesd_ioctl.h"

#define PORT            9000
#define BACKLOG         1
#define DEVICE_FILE     "/dev/aesdchar"
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
    if (server_fd >= 0) close(server_fd);
    closelog();
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

    char buf[BUF_SIZE] = {0};
    size_t buf_len = 0;

    int dev_fd = open(DEVICE_FILE, O_RDWR);
    if (dev_fd < 0) {
        syslog(LOG_ERR, "Failed to open %s", DEVICE_FILE);
        close(conn_fd);
        return NULL;
    }

    while (!do_exit) {
        ssize_t r = read(conn_fd, buf + buf_len, BUF_SIZE - buf_len);
        if (r <= 0) break;
        buf_len += r;

        char *newline;
        while ((newline = memchr(buf, '\n', buf_len)) != NULL) {
            size_t msg_len = newline - buf + 1;
            buf[msg_len] = '\0';

            // Check for ioctl command
            if (strncmp(buf, "AESDCHAR_IOCSEEKTO:", strlen("AESDCHAR_IOCSEEKTO:")) == 0) {
                unsigned int cmd = 0, offset = 0;
                if (sscanf(buf + strlen("AESDCHAR_IOCSEEKTO:"), "%u,%u", &cmd, &offset) == 2) {
                    struct aesd_seekto seekto = {.write_cmd = cmd, .write_cmd_offset = offset};
                    if (ioctl(dev_fd, AESDCHAR_IOCSEEKTO, &seekto) != 0) {
                        syslog(LOG_ERR, "IOCTL AESDCHAR_IOCSEEKTO failed: %s", strerror(errno));
                    }
                }
            } else {
                // Regular write
                write(dev_fd, buf, msg_len);
            }

            // Read from device and send back
            lseek(dev_fd, 0, SEEK_CUR);  // Ensure we're reading from the current file pos
            char read_buf[BUF_SIZE];
            ssize_t read_bytes;
            while ((read_bytes = read(dev_fd, read_buf, sizeof(read_buf))) > 0) {
                if (write(conn_fd, read_buf, read_bytes) < 0) break;
            }

            // Shift remaining buffer
            memmove(buf, buf + msg_len, buf_len - msg_len);
            buf_len -= msg_len;
        }
    }

    syslog(LOG_INFO, "Closed connection from %s", ip);
    close(dev_fd);
    close(conn_fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    bool daemon_mode = false;
    if (argc == 2 && strcmp(argv[1], "-d") == 0) daemon_mode = true;

    struct sigaction sa = {.sa_handler = signal_handler};
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    atexit(cleanup);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(PORT)};
    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, BACKLOG);

    if (daemon_mode && fork() > 0) exit(EXIT_SUCCESS);

    while (!do_exit) {
        int *pconn_fd = malloc(sizeof(*pconn_fd));
        *pconn_fd = accept(server_fd, NULL, NULL);
        if (*pconn_fd < 0) {
            free(pconn_fd);
            if (errno == EINTR) break;
            continue;
        }
        client_thread_t *t = calloc(1, sizeof(*t));
        pthread_create(&t->tid, NULL, connection_handler, pconn_fd);
        TAILQ_INSERT_TAIL(&thread_head, t, entries);
    }

    client_thread_t *t;
    while (!TAILQ_EMPTY(&thread_head)) {
        t = TAILQ_FIRST(&thread_head);
        pthread_join(t->tid, NULL);
        TAILQ_REMOVE(&thread_head, t, entries);
        free(t);
    }

    return 0;
}
