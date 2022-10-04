#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/epoll.h>
#include <fcntl.h>

#define PORT "8080"
#define BACKLOG 10
#define MAX_EVENTS 10

int get_listen_socket();

int setnonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {

    /* socket(), setnonblocking(), bind(), listen() */
    int listen_fd = get_listen_socket();
    if (listen_fd == -1) {
        perror("failed to get listen socket");
        exit(1);
    }

    //create a new epoll instance. epoll_create1 with flags = 0 is the same as epoll_create.
    int flags = 0;
    int epoll_fd = epoll_create1(flags);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(listen_fd);
        exit(1);
    }

    // edge triggered (EPOLLET) vs level triggered (default)
    // suppose 5kb of data is written to a socket and you can read only 1kb at a time.
    // when epoll_wait is called:
    //  edge-triggered: will block until a new write, leaving 4kb to be read.
    //                  This means that in ET mode, you must ensure you read until receiving a EAGAIN or EWOULDBLOCK
    //  level-triggered: will not block until the 4kb of data is read and read would return EAGAIN or EWOULDBLOCK

    //Add our listen socket to the epoll interest list to poll for read events
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;        // read operations, level triggered is assumed
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1) {
        perror("epoll_ctl add listen_fd failed");
        close(epoll_fd);
        close(listen_fd);
        exit(1);
    }

    struct epoll_event events[MAX_EVENTS];
    for (;;) {
        /*
         * timeout (milliseconds) of -1 mean block indefinitely
         * timeout of 0 means return immediately even if no fds are ready
         */
        int timeout = -1;
        int num_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout);
        if (num_fds == -1) {
            perror("epoll_wait");
            close(epoll_fd);
            close(listen_fd);
            exit(1);
        }

        //and now take a look at the events. if there is more than 10 events, then the
        //next call to epoll_wait will simply return the next set of waiting events without
        //blocking.
        for (int n = 0; n < num_fds; ++n) {

            // Of course, our listen socket is going to need to accept
            // our connections still. Except since now accept will not block, we rely
            // on epoll_wait to block and let us know when connections are ready to accept
            if (events->data.fd == listen_fd) {
                struct sockaddr_storage client_addr;
                socklen_t client_addr_len = sizeof(struct sockaddr_storage);
                int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);
                if (client_fd == -1) {
                    perror("accept");
                    close(listen_fd);
                    close(epoll_fd);
                    exit(1);
                }
                setnonblocking(client_fd);
                ev.events = EPOLLIN;
                ev.data.fd = client_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
                    perror("epoll_ctl: add client_fd");
                    close(client_fd);
                    close(listen_fd);
                    close(epoll_fd);
                    exit(1);
                }
            }

            else {
                // do events on the client_fd
                // remember that if you switch to edge-triggered mode, you must guarantee finishing
                // all of the operation as bytes remaining in the read buffer will not end up triggering
                // a new event should you be unable to read them all at once.

                //do_use_fd(events[n].data.fd);
            }

        }
    }

    return 0;
}


int get_listen_socket() {
    int rv = 0;

    struct addrinfo hints = {};
    struct addrinfo *bind_addr_list;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    rv = getaddrinfo(NULL, PORT, &hints, &bind_addr_list);
    if (rv != 0) {
        return rv;
    }

    int yes = 1;
    int listen_fd = 0;
    struct addrinfo *tmp;
    for (tmp = bind_addr_list; tmp != NULL; tmp = tmp->ai_next) {
        if ((listen_fd = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol)) == -1) {
            perror("Failed to get socket");
            continue;
        }

        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("failed to set socket reusable");
            exit(1);
        }

        if (bind(listen_fd, tmp->ai_addr, tmp->ai_addrlen) == -1) {
            close(listen_fd);
            perror("failed to bind address to socket");
            continue;
        }

        break;
    }
    if (tmp == NULL) {
        perror("failed to bind any address to socket");
        if (listen_fd != -1) {
            close(listen_fd);
        }
        freeaddrinfo(bind_addr_list);
        exit(1);
    }
    freeaddrinfo(bind_addr_list);

    // according to man epoll, the listen_fd should be nonblocking, so this is a new bit
    //from the basic socket echo
    if (setnonblocking(listen_fd) == -1) {
        perror("set nonblocking");
        close(listen_fd);
        exit(1);
    }
    //end of new from socket echo

    if (listen(listen_fd, BACKLOG) == -1) {
        perror("failed to begin listening");
        close(listen_fd);
        exit(1);
    }

    return listen_fd;
}
