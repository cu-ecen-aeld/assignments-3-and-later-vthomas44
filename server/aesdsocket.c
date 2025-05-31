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

#define PORT            9000
#define BACKLOG         1
#define DATA_FILE       "/var/tmp/aesdsocketdata"
#define BUF_SIZE        1024

static int server_fd = -1;
static volatile sig_atomic_t do_exit = 0;

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

    while (!do_exit) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);
        int conn_fd;

        conn_fd = accept(server_fd, (struct sockaddr *)&client, &client_len);
        if (conn_fd < 0) {
            if (errno == EINTR && do_exit) {
                break;
            }
            perror("accept");
            continue;
        }

        char *client_ip = inet_ntoa(client.sin_addr);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        FILE *df = fopen(DATA_FILE, "a+");
        if (!df) {
            perror("fopen");
            close(conn_fd);
            continue;
        }

        char buf[BUF_SIZE];
        while (true) {
            ssize_t r = read(conn_fd, buf, sizeof(buf));
            if (r <= 0) {
                break;
            }
            for (ssize_t i = 0; i < r; i++) {
                fputc(buf[i], df);
                if (buf[i] == '\n') {
                    fflush(df);
                    fseek(df, 0, SEEK_SET);

                    int c;
                    while ((c = fgetc(df)) != EOF) {
                        if (write(conn_fd, &c, 1) < 0) {
                            perror("write");
                            break;
                        }
                    }
                }
            }
        }

        fclose(df);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        close(conn_fd);
    }
    return 0;
}
