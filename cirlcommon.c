#include "cirlcommon.h"

// get sockaddr, IPv4 or IPv6:
void *getaddr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int sendall(int sock, const void *buffer, size_t len, int flags) {
    ssize_t bytes;
    const char *pos = buffer;

    while (len > 0) {
        bytes = send(sock, (void *)pos, len, flags);
        if (bytes <= 0) {
            return -1;
        }
        pos += bytes;
        len -= bytes;
    }

    return 0;
}

int recvall(int sock, const void *buffer, size_t len, int flags) {
    ssize_t bytes;
    const char *pos = buffer;

    while (len > 0) {
        bytes = recv(sock, (void *)pos, len, flags);
        if (bytes <= 0) {
            return -1;
        }
        pos += bytes;
        len -= bytes;
    }

    return 0;
}

// sometimes we need to cancel threads from inside a thread included in tids.
// we do not want to cancel that thread while it has the mutex locked,
// so we call this function with exclude = (tids index of calling thread)
// and then execute pthread_exit after this function returns
void pthread_cancel_all(pthread_t *tids, int exclude, int count, pthread_mutex_t *mutex) {
    int i;

    pthread_mutex_lock(&(*mutex));
    for (i = 0; i < count; i++) {
        if (i != exclude) {
            pthread_cancel(tids[i]);
        }
    }
    pthread_mutex_unlock(&(*mutex));
}

void cirlkill(int pid, int status) {
    kill(pid, SIGTERM);
    exit(status);
}

void sigterm(int sig) {
    exit(EXIT_SUCCESS);
}