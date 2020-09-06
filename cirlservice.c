#include "cirlcommon.h"

#include <math.h>

#define THREAD_COUNT 2
#define max(a,b) (a > b) ? a : b

char RGB[] = {35, 175, 133};
float EXP = 2.0;
int USE_SIG = 0; // sigmoid mode on/off

int sigmoid(int x) {
    return floor(1000 / (1 + exp(-0.013 * (x - 500))));
}

int color(char *colorstr) {
    char *r = NULL;
    char *temp = strtok_r(colorstr, " ", &r);
    int rgb[3], i;

    for (i = 0; i < 3; i++) {
        temp = strtok_r(NULL, " ", &r);
        if (temp != NULL) {
            rgb[i] = atoi(temp);
            if (rgb[i] < 0 || rgb[i] > 255) {
                fprintf(stderr, "Bad color value\n");
                return -1;
            }
        }
        else {
            fprintf(stderr, "Bad color value\n");
            return -1;
        }
    }

    for (i = 0; i < 3; i++) {
        RGB[i] = rgb[i];
    }

    return 0;
}

int amp(char *ampstr) {
    char *r = NULL;
    char *temp = strtok_r(ampstr, " ", &r);
    float a;

    temp = strtok_r(NULL, " ", &r);
    if (temp != NULL) {
        a = atof(temp);
        if (a < 0.0 || a > 5.0) {
            fprintf(stderr, "Bad amp value\n");
            return -1;
        }
    }
    else {
        fprintf(stderr, "Bad amp value\n");
        return -1;
    }

    EXP = a;

    return 0;
}

int setsigmoid(char *sigstr) {
    char *r = NULL;
    char *temp = strtok_r(sigstr, " ", &r);
    int i;

    temp = strtok_r(NULL, " ", &r);
    if (temp != NULL) {
        i = atof(temp);
        if (i != 0 && i != 1) {
            fprintf(stderr, "Bad setsigmoid value\n");
            return -1;
        }
    }
    else {
        fprintf(stderr, "Bad setsigmoid value\n");
        return -1;
    }

    USE_SIG = i;

    return 0;
}

int processline(char *line) {
    int sum = 0, average = 0, amplitude = 0, brightness, temp_int;
    char delim[] = " ", *temp = calloc(8, sizeof(char));

    if (line != NULL) {
        // get next line and feed into temp
        temp = strtok(line, delim);

        while (temp != NULL) {
            temp_int = atoi(temp);
            // sum up fifo line values
            sum += temp_int;

            // get the peak value of this waveform
            amplitude = (amplitude > temp_int) ? amplitude : temp_int;

            temp = strtok(NULL, delim);
        }

        sum += atoi(line);
        average = sum / 200;

        free(temp);
        if (USE_SIG) {
            brightness = sigmoid(amplitude);
        }
        else {
            brightness = floorl((powl(amplitude, EXP) / powl(1000, EXP - 1)));
        }
        return brightness;
    }

}

// handle in-execution menu
void menu(char *menu_opt) {
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
            
            break;
    }
}

char *cavaloop(int cava_sock) {
    char *line = calloc(BUFFER_SIZE, sizeof(char)), *menu_opt = calloc(MENUOPT_SIZE, sizeof(char));
    char *temp = calloc(RETMSG_SIZE * 2, sizeof(char));
    char *retmsg = calloc(32, sizeof(char));
    int amplitude, r, g, b;

    // read data from socket
    while (1) {
        if (recvall(cava_sock, line, BUFFER_SIZE, 0) != 0 ||
            recvall(cava_sock, menu_opt, MENUOPT_SIZE, 0) != 0) {
            return "Client disconnected";
        }

        menu(menu_opt);

        amplitude = floor(processline(line) * 0.255);
        r = floor(RGB[0] * ((double)amplitude / 255));
        g = floor(RGB[1] * ((double)amplitude / 255));
        b = floor(RGB[2] * ((double)amplitude / 255));
        sprintf(temp, "/usr/local/bin/pigs p 17 %d p 22 %d p 24 %d", r, g, b);
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
        fprintf(stderr, "Could not bind\n");
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
        // turn LEDs off when client disconnects, reset some values
        system("/usr/local/bin/pigs p 17 0 p 22 0 p 24 0");
        RGB[0] = 35; RGB[1] = 175; RGB[2] = 133;
        EXP = 2.0;
        USE_SIG = 0;

        close(new_sock);
    }

    return 0;
}
