#include "cirlcommon.h"
#include <ctype.h>
#include <sys/mman.h>

#define THREAD_COUNT 2

// structs
struct cirl_info {
    char *conf; // configuration file name
    char *pi_ip; // ip address of the raspberry pi
    char *fifo_name; // fifo file name
};

// headers
void *menuloop(void *arg);

void *fifoloop(void *arg);

// global variables
pthread_t tids[THREAD_COUNT];
void *(*pthread_funcs[])(void *) = {menuloop, fifoloop};
char *menu_opt = NULL;
struct cirl_info *cinfo = NULL;
int cancel_flag = 0;
char *exitmsg = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

char helpmsg[] = \
"\ncava-irl commands: \n\
------------------ \n\
Change color: \"c (num) (num) (num)\" (must be RGB value)\n\
\n\
Change constraster: \"a (num)\" (between 0 and 5)\n\
   - Makes higher peaks brighter, lower peaks dimmer\n\
\n\
Toggle sigmoid mode: \"s (0 or 1)\"\n\
   - Changes the way brightness is calculated. See the github page for details\n\
\n\
Show this help message: \"h\"\n\
\n\
Exit cava-irl: \"q\"\n\
   - NOTE: This does not exit cirlservice; you must turn it off manually within the pi\n\n";


int getsock(char *ip, char *port) {
    struct addrinfo hints, *res, *s;
    int errcode, sock, bytes_sent, len;
    char name[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // listen for any address
    if ((errcode = getaddrinfo(ip, port, &hints, &res)) != 0) {
        fprintf(stderr, "Error getting address info: %s\n", gai_strerror(errcode));
        return -1;
    }

    // iterate through received addresses and bind to first available
    for (s = res; s != NULL; s = s->ai_next) {
        if ((sock = socket(s->ai_family, s->ai_socktype, s->ai_protocol)) == -1) {
            continue;
        }

        if (connect(sock, s->ai_addr, s->ai_addrlen) == -1) {
            close(sock);
            continue;
        }

        break;
    }

    if (s == NULL) {
        perror("Could not connect to host");
        return -1;
    }

    // get name of client
    if (getpeername(sock, s->ai_addr, &(s->ai_addrlen)) == 0) {
        inet_ntop(s->ai_family, getaddr(s->ai_addr), name, sizeof(name));
        printf("Bound to host (%s); sending data...\n", name);
    }
    else {
        perror("Could not resolve client name");
        return -1;
    }

    freeaddrinfo(res);

    return sock;
}

// handle in-execution menu
void *menuloop(void *arg) {
    char *temp = calloc(MENUOPT_SIZE, sizeof(char));
    size_t size = MENUOPT_SIZE;
    int i;

    sleep(1);
    printf("Enter commands below:\n");
    while (1) {
        if (menu_opt == NULL) {
            // we use a temporary variable to wait for user input because we would lock up
            // the fifoloop process otherwise
            fputs("> ", stdout);
            fflush(stdout);
            fgets(temp, MENUOPT_SIZE, stdin);

            pthread_mutex_lock(&mutex);
            menu_opt = temp;

            // check for valid input then wait for fifoloop to reset it
            switch(menu_opt[0]) {
                case ' ':
                case '\n':
                case '\0':
                    menu_opt = NULL;
                case 'a':
                case 'c':
                case 's':
                    // allows fifoloop to read
                    break;
                case 'h':
                    printf("%s", helpmsg);
                    menu_opt = NULL;
                    break;
                case 'q':
                    // wait for fifoloop to read and exit
                    free(menu_opt);
                    // prepare return message
                    exitmsg = "Option \'q\' was input";
                    cancel_flag = 1;
                    pthread_mutex_unlock(&mutex);
                    pthread_exit(NULL);
                    break;
                default:
                    printopterr("Invalid option code: " + menu_opt[0]);
                    printf("%s", helpmsg);
                    menu_opt = NULL;
                    break;
            }
            pthread_mutex_unlock(&mutex);
        }
        else {
            // wait 1ms for data to be processed; shouldn't happen too often
            usleep(1000);
        }
    }
}

// handle fifo data
void *fifoloop(void *arg) {
    char *conf = cinfo->conf;
    char *pi_ip = cinfo->pi_ip;
    char *fifo_name = cinfo->fifo_name;

    char *line;
    size_t readcode;
    int sockfd;
    FILE *fifo;

    // clear fifo from previous cava sessions
    remove(fifo_name);
    mkfifo(fifo_name, 0777);
    fifo = fopen(fifo_name, "r");

    if (fifo == NULL) {
        fprintf(stderr, "cava fifo file could not be created or opened\n");
        pthread_mutex_lock(&mutex);
        exitmsg = "Could not use fifo file";
        cancel_flag = 1;
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
    }

    line = calloc(BUFFER_SIZE, sizeof(char));
    // get socket on fifo data port
    if ((sockfd = getsock(pi_ip, PORT)) == -1) {
        fclose(fifo);
        free(line);
        pthread_mutex_lock(&mutex);
        exitmsg = "Socket error";
        cancel_flag = 1;
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
    }

    // send fifo data
    while (1) {
        readcode = fread(line, sizeof(char), BUFFER_SIZE, fifo);
        // data can be sent
        if (readcode > 0) {
            pthread_mutex_lock(&mutex);
            sendall(sockfd, line, BUFFER_SIZE, 0);
            if (menu_opt != NULL) {
                sendall(sockfd, menu_opt, MENUOPT_SIZE, 0);
                menu_opt = NULL;
            }
            else {
                sendall(sockfd, "n", MENUOPT_SIZE, 0);
            }
            pthread_mutex_unlock(&mutex);
        }
        // error occurred
        else if (readcode < 0) {
            fprintf(stderr, "Error occurred while reading from fifo: %s\n", strerror(ferror(fifo)));
            fclose(fifo);
            close(sockfd);
            free(line);
            pthread_mutex_lock(&mutex);
            exitmsg = "Fifo error";
            cancel_flag = 1;
            pthread_mutex_unlock(&mutex);
            pthread_exit(NULL);
        }
        // EOF (shouldn't happen)
        else {
            // wait a millisecond for more input
            usleep(1000);
        }
    }

}


int main(int argc, char *argv[]) {
    // misc
    int option, i, pid;
    // thread info
    pthread_attr_t attr[THREAD_COUNT];
    int pthread_status[THREAD_COUNT];

    // handle malloc
    if ((cinfo = malloc(sizeof(struct cirl_info))) == NULL) {
        perror("Malloc failure");
        exit(EXIT_FAILURE);
    }

    // handle pre-execution options
    while ((option = getopt(argc, argv, ":c:i:f:")) != -1) {
        switch (option) {
            case 'c':
                cinfo->conf = optarg;
                break;
            case 'i':
                cinfo->pi_ip = optarg;
                break;
            case 'f':
                cinfo->fifo_name = optarg;
                break;
            case ':':
                perror("Option needs a value");
                free(cinfo);
                exit(EXIT_FAILURE);
                break;
            case '?':
                fprintf(stderr, "Unknown option: %c\n", optopt);
                free(cinfo);
                exit(EXIT_FAILURE);
                break;
        }
    }

    // two processes: one for cava, one for handling menu commands and fifo data
    pid = fork();
    // error
    if (pid == -1) {
        perror("Process creation error");
        free(cinfo);
        exit(EXIT_FAILURE);
    }
    // child: cava
    else if (pid == 0) {
        char *conf = cinfo->conf;
        char *arg = calloc(strlen(conf) + 4, sizeof(char));

        strcpy(arg, "-p ");
        strcat(arg, conf);

        signal(SIGTERM, sigterm);

        // NOTE: this is probably very bad, but closing stdin seems to be the only way to
        // allow user input in the menuloop
        close(fileno(stdin));
        if (execlp("cava", arg, NULL) == -1) {
            fprintf(stderr, "Could not find cava executable file. Is it installed and in the right place \
                            (/usr/local/bin/cava)?\n");
            free(arg);
            return -1;
        }
    }
    // parent: menu + fifo
    else {
        printf("Initializing processes...\n");

        // locking so we can fully/safely initialize tids
        pthread_mutex_lock(&mutex);
        for (i = 0; i < THREAD_COUNT; i++) {
            pthread_attr_init(&attr[i]);
            pthread_status[i] = pthread_create(&tids[i], &attr[i], pthread_funcs[i], NULL);
            pthread_attr_destroy(&attr[i]);
        }
        pthread_mutex_unlock(&mutex);

        // handle thread creation failure
        for (i = 0; i < THREAD_COUNT; i++) {
            if (pthread_status[i] != 0) {
                fprintf(stderr, "Could not create threads: %s\n", strerror(pthread_status[i]));
                pthread_cancel_all(tids, -1, THREAD_COUNT, &mutex);
                free(cinfo);
                cirlkill(pid, EXIT_FAILURE);
            }
        }

        printf("Processes successfully initialized.\n");

        while (1) {
            for (i = 0; i < THREAD_COUNT; i++) {
                usleep(1000);
                if (cancel_flag) {
                    printf("Exiting: %s\n", exitmsg);
                    free(cinfo);
                    pthread_cancel_all(tids, cancel_flag, THREAD_COUNT, &mutex);
                    cirlkill(pid, EXIT_SUCCESS);
                }
            }
        }
    }

    return 0;
}
