#include <ctype.h>
#include <sys/mman.h>
#include "cirlcommon.h"
#include "led.h"
#include "tomlc99/toml.h"

#define THREAD_COUNT 2
#define STR(x) #x // magic directive that prints out values of macros
#define DEFAULT_CONFIG_PATH "/.config/cirl/cirl.toml"

#define max(a,b) (a > b) ? a : b

// structs
struct cirl_info {
    char *cava_conf; // configuration file name
    char *pi_ip; // ip address of the raspberry pi
    char *fifo_name; // fifo file name
};

// headers
void *menuloop(void *arg);

void *fifoloop(void *arg);

// led variables
unsigned char *RGB, **gradient; // rgb colors
float AMP = 2.0; // see github page for this
char USE_SIG = 0, GRADIENT = 0; // sigmoid mode on/off, gradient on/off
int gradient_count;

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
\"a (num)\" - set contraster (between 1.0000 and 5.0000)\n\
   * makes higher peaks brighter, lower peaks dimmer\n\
\"c (num) (num) (num)\" - set color (must be RGB value)\n\
\"g (0 or 1)\" - turn gradient mode on or off\n\
\"h\" - show this help message\n\
\"s (0 or 1)\" - toggle sigmoid mode\n\
   * changes the way brightness is calculated. See the github page for details\n\
\"q\" - exit cirl\n\
   * NOTE: This does not stop the cirlservice daemon\n\n";

char init_helpmsg[] = \
"\ncava-irl options: \n\
----------------- \n\
\n\
-c (path/to/cirl/config), where (path/to/cirl/config) is the path to the TOML config \n\
    file that cirl should use (uses ~/.config/cirl/cirl.toml by default) \n\
-h, displays this message then terminates \n\
\n";



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
            case 'g':
                GRADIENT = !GRADIENT;
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
    char *cava_conf = cinfo.cava_conf;
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
        fprintf(stderr, "cava fifo file could not be created or opened - %s\n", strerror(errno));
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
    char homedir[BUFFER_SIZE];
    // thread info
    pthread_attr_t attr[THREAD_COUNT];
    int pthread_status[THREAD_COUNT];
    // cirl config info
    char *toml_path, temp_str[MAX_GRADIENT_COUNT_SLEN], gradient_id[MAX_GRADIENT_COUNT_IDLEN];
    FILE *toml_fp = NULL;
    toml_table_t *cirl_toml;
    toml_table_t *server_toml, *cava_toml, *colors_toml;
    toml_datum_t ip_server_toml, port_server_toml, config_cava_toml, fifo_cava_toml, gradient_colors_toml, temp;
    toml_array_t *solid_colors_toml;
    char badval = 0;
    char errbuf[256];

    // handle pre-execution options
    while ((option = getopt(argc, argv, ":c:h")) != -1) {
        switch (option) {
            case 'c':
                toml_path = optarg;
                if (!(toml_fp = fopen(toml_path, "r"))) {
                    fprintf(stderr, "Cannot open \"%s\" toml file - %s", optarg, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
                printf("%s", init_helpmsg);
                exit(EXIT_SUCCESS);
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

    // read and parse toml file
    // use default toml file if given one invalid or not provided
    if (toml_fp == NULL) {
        snprintf(homedir, BUFFER_SIZE, getenv("HOME"));
        toml_path = strcat(homedir, DEFAULT_CONFIG_PATH);
        if (!(toml_fp = fopen(toml_path, "r"))) {
            printf("%s\n", toml_path);
            fprintf(stderr, "Cannot use/find default toml file (is it %s ?) - %s\n", STR(DEFAULT_CONFIG_PATH), strerror(errno));
            exit(EXIT_FAILURE);
        }
        printf("cirl.toml file not provided or invalid, using default...\n");
    }
    cirl_toml = toml_parse_file(toml_fp, errbuf, sizeof(errbuf));
    fclose(toml_fp);
    if (!cirl_toml) {
        fprintf(stderr, "Cannot parse toml file - %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // read toml tables
    server_toml = toml_table_in(cirl_toml, "server");
    if (!server_toml) {
        fprintf(stderr, "Missing [server] table in toml file\n");
        exit(EXIT_FAILURE);
    }
    cava_toml = toml_table_in(cirl_toml, "cava");
    if (!cava_toml) {
        fprintf(stderr, "Missing [cava] table in toml file\n");
        exit(EXIT_FAILURE);
    }
    colors_toml = toml_table_in(cirl_toml, "colors");
    if (!colors_toml) {
        fprintf(stderr, "Missing [colors] table in toml file\n");
        exit(EXIT_FAILURE);
    }

    // read required values from toml tables
    ip_server_toml = toml_string_in(server_toml, "ip");
    if (!ip_server_toml.ok) {
        fprintf(stderr, "Missing [ip] field in [server] table in toml file\n");
        exit(EXIT_FAILURE);
    }
    else {
        cinfo.pi_ip = ip_server_toml.u.s;
    }
    port_server_toml = toml_int_in(server_toml, "port");
    #ifdef PORT
    if (port_server_toml.ok) {
        #undef PORT
        #define PORT port_server_toml.u.i;
    }
    #endif
    if (!port_server_toml.ok) {
        printf("[port] field in [server] table in toml file missing, using default port %s\n", STR(PORT));
    }
    fifo_cava_toml = toml_string_in(cava_toml, "fifo");
    if (!fifo_cava_toml.ok) {
        fprintf(stderr, "Missing [fifo] field in [cava] table in toml file\n");
        exit(EXIT_FAILURE);
    }
    else {
        cinfo.fifo_name = fifo_cava_toml.u.s;
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
        // read cava config path
        config_cava_toml = toml_string_in(cava_toml, "config");
        if (!config_cava_toml.ok) {
            fprintf(stderr, "Missing [config] field in [cava] table in toml file\n");
            cirlkill(pid, EXIT_FAILURE);
        }
        else {
            cinfo.cava_conf = config_cava_toml.u.s;
        }

        char *conf = cinfo.cava_conf;
        char *arg = calloc(strlen(conf) + 4, sizeof(char));

        strcpy(arg, "-p ");
        strcat(arg, conf);

        signal(SIGTERM, sigterm);

        // NOTE: this is probably very bad, but closing stdin seems to be the only way to
        // allow user input in the menuloop
        close(STDIN_FILENO);
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

        // thread creation error check
        for (i = 0; i < THREAD_COUNT; i++) {
            if (pthread_status[i] != 0) {
                fprintf(stderr, "Could not create threads: %s\n", strerror(pthread_status[i]));
                pthread_cancel_all(tids, -1, THREAD_COUNT, &mutex);
                cirlkill(pid, EXIT_FAILURE);
            }
        }

        printf("Processes successfully initialized.\n");

        // read solid colors
        solid_colors_toml = toml_array_in(colors_toml, "solid");
        RGB = calloc(3, sizeof(char));
        if (solid_colors_toml) {
            for (int i = 0; i < 3; i++) {
                temp = toml_int_at(solid_colors_toml, i);
                if (!temp.ok) {
                    // TODO verify that errno is set appropriately
                    fprintf(stderr, "Bad value in [solid] field in [colors] table in toml file - %s\n", strerror(errno));
                    badval = 1;
                    break;
                }
                RGB[i] = temp.u.i;
            }
        }
        if (!solid_colors_toml || badval) {
            RGB[0] = 51;
            RGB[1] = 255;
            RGB[2] = 194;
            printf("[solid] field in [colors] table in toml file missing/invalid, using default values %d %d %d\n", RGB[0], RGB[1], RGB[2]);
        }
        badval = 0;

        // read gradient colors
        gradient_colors_toml = toml_bool_in(colors_toml, "gradient");
        if (!gradient_colors_toml.ok) {
            fprintf(stderr, "Missing [gradient] field in [colors] table in toml file\n");
            pthread_cancel_all(tids, cancel_flag, THREAD_COUNT, &mutex);
            free(RGB);
            cirlkill(pid, EXIT_FAILURE);
        }
        else {
            gradient_count = gradient_colors_toml.u.b;
        }

        printf("Loading gradient values...\n");
        gradient = calloc(MAX_GRADIENT_COUNT, sizeof(unsigned char *));
        for (int i = 0; i < MAX_GRADIENT_COUNT || !badval; i++) {
            snprintf(temp_str, MAX_GRADIENT_COUNT_SLEN, "%d", i);
            snprintf(gradient_id, MAX_GRADIENT_COUNT_IDLEN, "%s", strcat("gradient", temp_str));

            temp = toml_string_in(colors_toml, gradient_id);
            if (!temp.ok) {
                badval = 1;
            }
            else {
                gradient[i] = calloc(3, sizeof(unsigned char));
                gradient_count++;
                for (int j = 0; j < 3; j++) {
                    temp = toml_int_at(solid_colors_toml, i);
                    if (!temp.ok || temp.u.i > 255 || temp.u.i < 0) {
                        // TODO verify that errno is set appropriately
                        fprintf(stderr, "Bad value in [%s] field in [colors] table in toml file - %s\n", gradient_id, strerror(errno));
                        fprintf(stderr, "cirl will only use gradients preceding %s\n", gradient_id);
                        badval = 1;
                        free(gradient[i]);
                        gradient_count--;
                        break;
                    }
                    gradient[i][j] = temp.u.i;
                }
            }

            if (i == MAX_GRADIENT_COUNT - 1) {
                printf("Note: Only first %d gradient values will be used\n", MAX_GRADIENT_COUNT);
            }
        }
        printf("Gradient values loaded. Count: %d\n", gradient_count);

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
