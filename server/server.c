#include <stdio.h>
#include <netdb.h>

#define PORT "8080"
#define MAX_EVENTS 10

int get_listen_socket();

int main() {





    return 0;
}


int get_list_socket() {
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

}
