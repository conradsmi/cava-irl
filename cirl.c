#include <ctype.h>
#include <sys/mman.h>
#include "cirlcommon.h"
#include "led.h"

#define THREAD_COUNT 2

#define max(a,b) (a > b) ? a : b

// structs
struct cirl_info {
    char *conf; // configuration file name
    char *pi_ip; // ip address of the raspberry pi
    char *fifo_name; // fifo file name
};

// headers
void *menuloop(void *arg);

void *fifoloop(void *arg);

// led variables
unsigned char *RGB; // rgb color
float AMP = 2.0; // see github page for this
char USE_SIG = 0; // sigmoid mode on/off
// general variables
struct cirl_info cinfo;
int cancel_flag = 0;
char *menu_opt = NULL;
// thread variables
pthread_t tids[THREAD_COUNT];
void *(*pthread_funcs[])(void *) = {menuloop, fifoloop};
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// info for user
char *exitmsg = NULL;
char helpmsg[] = \
"\ncava-irl commands: \n\
------------------ \n\
Change color: \"c (num) (num) (num)\" (must be RGB value)\n\
\n\
Change constraster: \"a (num)\" (between 1.0000 and 5.0000)\n\
   - Makes higher peaks brighter, lower peaks dimmer\n\
\n\
Toggle sigmoid mode: \"s (0 or 1)\"\n\
   - Changes the way brightness is calculated. See the github page for details\n\
\n\
Show this help message: \"h\"\n\
\n\
Exit cirl: \"q\"\n\
   - NOTE: This does not exit cirlservice; you must turn it off manually within the pi by entering Ctrl+C\n\n";


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
        // we use a temporary variable to wait for user input because we would lock up
        // the fifoloop process otherwise
        fputs("> ", stdout);
        fflush(stdout);
        fgets(temp, MENUOPT_SIZE, stdin);
        fflush(stdin);

        pthread_mutex_lock(&mutex);
        menu_opt = temp;
        // check for valid input then wait for fifoloop to reset it
        switch(menu_opt[0]) {
            case ' ':
            case '\n':
            case '\0':
                printf("Enter \'h\' for help\n");
                break;
            case 'a':
                AMP = amp(menu_opt, AMP);
                break;
            case 'c':
                color(menu_opt, RGB);
                break;
            case 's':
                USE_SIG = setsigmoid(menu_opt, USE_SIG);
                break;
            case 'h':
                printf("%s", helpmsg);
                break;
            case 'q':
                free(menu_opt);
                exitmsg = "Option \'q\' was input";
                cancel_flag = 1;
                pthread_mutex_unlock(&mutex);
                pthread_exit(NULL);
                break;
            default:
                printf("Invalid option code: %c\n", menu_opt[0]);
                printf("%s", helpmsg);
                break;
        }
        menu_opt = NULL;
        pthread_mutex_unlock(&mutex);
    }
}

// handle fifo data
void *fifoloop(void *arg) {
    char *conf = cinfo.conf;
    char *pi_ip = cinfo.pi_ip;
    char *fifo_name = cinfo.fifo_name;

    char *line, *cmd;
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

    // get socket on fifo data port
    if ((sockfd = getsock(pi_ip, PORT)) == -1) {
        fclose(fifo);
        pthread_mutex_lock(&mutex);
        exitmsg = "Socket error";
        cancel_flag = 1;
        pthread_mutex_unlock(&mutex);
        pthread_exit(NULL);
    }

    line = calloc(BUFFER_SIZE, sizeof(char));
    cmd = calloc(CMD_SIZE, sizeof(char));
    // send fifo data
    while (1) {
        readcode = fread(line, sizeof(char), BUFFER_SIZE, fifo);
        // data can be sent
        if (readcode > 0) {
            pthread_mutex_lock(&mutex);
            getcmd(line, RGB, AMP, USE_SIG, cmd);
            sendall(sockfd, cmd, CMD_SIZE, 0);
            pthread_mutex_unlock(&mutex);
        }
        // error occurred
        else if (readcode < 0) {
            fprintf(stderr, "Error occurred while reading from fifo: %s\n", strerror(ferror(fifo)));
            fclose(fifo);
            close(sockfd);
            free(line);
            free(cmd);
            pthread_mutex_lock(&mutex);
            exitmsg = "Fifo error";
            cancel_flag = 1;
            pthread_mutex_unlock(&mutex);
            pthread_exit(NULL);
        }
        // EOF (shouldn't happen)
        else {
            // wait 2.5ms for more input
            usleep(2500);
        }
    }

}


int main(int argc, char *argv[]) {
    int option, i, pid;
    // thread info
    pthread_attr_t attr[THREAD_COUNT];
    int pthread_status[THREAD_COUNT];

    // handle pre-execution options
    while ((option = getopt(argc, argv, ":c:i:f:")) != -1) {
        switch (option) {
            case 'c':
                cinfo.conf = optarg;
                break;
            case 'i':
                cinfo.pi_ip = optarg;
                break;
            case 'f':
                cinfo.fifo_name = optarg;
                break;
            case ':':
                perror("Option needs a value");
                exit(EXIT_FAILURE);
                break;
            case '?':
                fprintf(stderr, "Unknown option: %c\n", optopt);
                exit(EXIT_FAILURE);
                break;
        }
    }

    // two processes: one for cava, one for handling menu commands and fifo data
    pid = fork();
    // fork error check
    if (pid == -1) {
        perror("Process creation error");
        exit(EXIT_FAILURE);
    }
    // child: cava
    else if (pid == 0) {
        char *conf = cinfo.conf;
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
        RGB = calloc(3, sizeof(char));
        RGB[0] = 51;
        RGB[1] = 255;
        RGB[2] = 194;

        // locking so we can fully/safely initialize tids
        pthread_mutex_lock(&mutex);
        for (i = 0; i < THREAD_COUNT; i++) {
            pthread_attr_init(&attr[i]);
            pthread_status[i] = pthread_create(&tids[i], &attr[i], pthread_funcs[i], NULL);
            pthread_attr_destroy(&attr[i]);
        }
        pthread_mutex_unlock(&mutex);

        // thread creation error check
        for (i = 0; i < THREAD_COUNT; i++) {
            if (pthread_status[i] != 0) {
                fprintf(stderr, "Could not create threads: %s\n", strerror(pthread_status[i]));
                pthread_cancel_all(tids, -1, THREAD_COUNT, &mutex);
                free(RGB);
                cirlkill(pid, EXIT_FAILURE);
            }
        }

        printf("Processes successfully initialized.\n");

        // sleep until cancel flag is set
        while (1) {
            usleep(2500);
            if (cancel_flag) {
                printf("Exiting: %s\n", exitmsg);
                pthread_cancel_all(tids, cancel_flag, THREAD_COUNT, &mutex);
                free(RGB);
                cirlkill(pid, EXIT_SUCCESS);
            }
        }
    }

    return 0;
}
