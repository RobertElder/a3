
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "0"  

#define BACKLOG 10     

#define LINE_BUFFER_SIZE 1024

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    int num_bytes;
    char typed_line[LINE_BUFFER_SIZE+1];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

	socklen_t len = sizeof(*(p->ai_addr));
	if (getsockname(sockfd, ((struct sockaddr *)p->ai_addr), &len) == -1){
	    perror("getsockname");
	}else{
	    char hostname[100];
            struct hostent *hp;
	    gethostname(hostname, 99);
            hp = gethostbyname(hostname);
            printf("SERVER_ADDRESS %s\n", hp->h_name);
            printf("SERVER_PORT %d\n", ntohs(((struct sockaddr_in*)p->ai_addr)->sin_port));
	    /* Flush the output so we can read it from the file */
	    fflush(stdout);
	}

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);

        if (!fork()) {
            close(sockfd);
            if ((num_bytes = recv(new_fd, typed_line, LINE_BUFFER_SIZE -1, 0)) == -1)
                perror("send");
	    typed_line[num_bytes] = '\0';
	    printf("Received: %s\n",typed_line);
	    if(send(new_fd, typed_line, strlen(typed_line), 0) == -1){
	        perror("send");
	    }
            close(new_fd);
            exit(0);

        }
        close(new_fd);
    }

    return 0;
}
