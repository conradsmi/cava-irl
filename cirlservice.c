#include "cirlcommon.h"
#include "syslog.h"
#include <math.h>

char *cavaloop(int cava_sock) {
    char *temp = calloc(RETMSG_SIZE * 2, sizeof(char));

    // read data from socket
    while (1) {
        if (recvall(cava_sock, temp, CMD_SIZE, 0) != 0) {
            free(temp);
            return "Client disconnected";
        }

        system(temp);
    }

    // this should never happen
    return "Execution ended abruptly";
}

int main(int argc, char *argv[]) {
    struct addrinfo hints, *res, *s;
    int errcode, sock, new_sock;
    char name[INET6_ADDRSTRLEN];
    int i;
    char *retmsg;

    // daemonize
    int sid, pid = fork();

    if (pid == -1) {
        perror("Process creation error");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0) {
        umask(0);

        sid = setsid();
        if (sid < 0) {
            perror("Session creation error");
            exit(EXIT_FAILURE);
        }

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        chdir("/");
        openlog("cirlserv", LOG_NDELAY, LOG_USER);

        // don't block context switches
        sleep(1);
        syslog(LOG_INFO, "Beginning cirlserv daemon...");

        signal(SIGTERM, sigterm);

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        // listen for any address
        if ((errcode = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
            syslog(LOG_ERR, "Error getting address info: %s\n", gai_strerror(errcode));
            exit(EXIT_FAILURE);
        }

        // iterate through received addresses and bind to first available
        for (s = res; s != NULL; s = s->ai_next) {
            if ((sock = socket(s->ai_family, s->ai_socktype, s->ai_protocol)) == -1) {
                continue;
            }

            if (bind(sock, s->ai_addr, s->ai_addrlen) == 0) {
                break;
            }

            close(sock);
        }

        // although this line existed at this point in the manpage for getaddrinfo, freeing res
        // frees s->ai_family as well, which caused it to become undefined; inet_ntop thus did not work
        //freeaddrinfo(res);

        if (s == NULL) {
            syslog(LOG_ERR, "Could not bind: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (listen(sock, 1) == -1) {
            syslog(LOG_ERR, "Could not listen for connections: %s", strerror(errno));
            freeaddrinfo(res);
            exit(EXIT_FAILURE);
        }

        while(1) {
            syslog(LOG_INFO, "Ready for new connections!");
            new_sock = accept(sock, s->ai_addr, &(s->ai_addrlen));

            if (new_sock == -1) {
                syslog(LOG_ERR, "Could not accept connection: %s", strerror(errno));
                continue;
            }

            if (getpeername(new_sock, s->ai_addr, &(s->ai_addrlen)) == 0) {
                inet_ntop(s->ai_family, getaddr(s->ai_addr), name, INET_ADDRSTRLEN);
                syslog(LOG_INFO, "Bound to client (%s); reading data and feeding it to GPIOs...", name);
            }
            else {
                syslog(LOG_INFO, "Could not resolve client name: %s", strerror(errno));
                freeaddrinfo(res);
                exit(EXIT_FAILURE);
            }

            retmsg = cavaloop(new_sock);
            syslog(LOG_INFO, "Closing connection with %s: %s", name, retmsg);
            // turn LEDs off when client disconnects
            system("/usr/local/bin/pigs p 17 0 p 22 0 p 24 0");

            close(new_sock);
        }

        freeaddrinfo(res);
        syslog(LOG_INFO, "Exiting cirlserv...");
        closelog();
    }

    exit(EXIT_SUCCESS);

    return 0;
}
