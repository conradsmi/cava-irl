#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>

#include <arpa/inet.h>
#include <netdb.h>

#define BUFFER_SIZE 2048
#define CMD_SIZE 64
#define RETMSG_SIZE 32
#define MENUOPT_SIZE 16
#define PORT "32816"

void *getaddr(struct sockaddr *sa);

int sendall(int sock, const void *buffer, size_t len, int flags);

int recvall(int sock, const void *buffer, size_t len, int flags);

void pthread_cancel_all(pthread_t *tids, int exclude, int count, pthread_mutex_t *mutex);

void cirlkill(int pid, int status);

void sigterm(int sig);