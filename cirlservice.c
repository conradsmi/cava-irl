#include "cirlcommon.h"

#include <math.h>

#define THREAD_COUNT 2

// handle in-execution menu
// will be removed soon
/*void menu(char *menu_opt) {
    switch(menu_opt[0]) {
        case 'a':
            if(!amp(menu_opt)) {
                printf("Changing amplifier... %.4lf\n", EXP);
            }
            break;
        case 'c':
            if(!color(menu_opt)) {
                printf("Changing color... %d %d %d\n", RGB[0], RGB[1], RGB[2]);
            }
            break;
        case 's':
            if(!setsigmoid(menu_opt)) {
                printf("Turning sigmoid mode... %s\n", USE_SIG ? "on" : "off");
            }
            break;
        default:
            // shouldn't happen
            break;
    }
}*/

char *cavaloop(int cava_sock) {
    char *temp = calloc(RETMSG_SIZE * 2, sizeof(char));

    // read data from socket
    while (1) {
        if (recvall(cava_sock, temp, BUFFER_SIZE, 0) != 0) {
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

    signal(SIGTERM, sigterm);

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // listen for any address
    if ((errcode = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
        fprintf(stderr, "Error getting address info: %s\n", gai_strerror(errcode));
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

    freeaddrinfo(res);

    if (s == NULL) {
        perror("Could not bind");
        exit(EXIT_FAILURE);
    }
    if (listen(sock, 1) == -1) {
        perror("Could not listen for connections");
        exit(EXIT_FAILURE);
    }

    while(1) {
        printf("Ready for new connections!\n");
        new_sock = accept(sock, s->ai_addr, &(s->ai_addrlen));

        if (new_sock == -1) {
            perror("Could not accept connection");
            continue;
        }

        // get name of client
        // TODO fix name
        if (getpeername(new_sock, s->ai_addr, &(s->ai_addrlen)) == 0) {
            inet_ntop(s->ai_family, getaddr(s->ai_addr), name, sizeof(name));
            printf("Bound to client (%s); reading data and feeding it to GPIOs...\n", name);
        }
        else {
            perror("Could not resolve client name");
            exit(EXIT_FAILURE);
        }

        retmsg = cavaloop(new_sock);
        printf("Closing connection with %s: %s\n", name, retmsg);
        // turn LEDs off when client disconnects
        system("/usr/local/bin/pigs p 17 0 p 22 0 p 24 0");

        close(new_sock);
    }

    return 0;
}
