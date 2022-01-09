#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define FALSE 0
#define TRUE 1
#define port 21

int debugMode = TRUE;

void processResp(int sockfd);

int main(int argc, char *argv[]) {
    unsigned char *end, *arg, *host, *url_path, *user, *password = NULL, *ip_addr;
    int isAnonymous = TRUE;
    struct hostent *h;

    int sockfd;
    struct sockaddr_in server_addr;
    struct sockaddr_in resp_addr;
    char respCode[3];

    char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
    size_t bytes;

    printf("\n");
    if (argc != 2) {
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    arg = argv[1];

    if (!strstr(arg, "ftp://")) {
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    arg += + strlen("ftp://");

    if (end = strstr(arg, ":")) {
        isAnonymous = FALSE;

        user = (char *) malloc(strlen(arg) - strlen(end) + 1);
        memcpy(user, arg, strlen(arg) - strlen(end));
        user[strlen(arg) - strlen(end)] = '\0';

        if(debugMode) if (user) printf("User: %s\n", user);

        arg += + strlen(user) + 1;

        if (end = strstr(arg, "@")) {
            password = (char *) malloc(strlen(arg) - strlen(end));
            memcpy(password, arg, strlen(arg) - strlen(end));
            password[strlen(arg) - strlen(end)] = '\0';

            if(debugMode) if (password) printf("Password: %s\n", password);

            arg += + strlen(password) + 1;
        }
    }

    if (end = strstr(arg, "/")) {
        host = (char *) malloc(strlen(arg) - strlen(end) + 1);
        memcpy(host, arg, strlen(arg) - strlen(end) + 1);
        host[strlen(arg) - strlen(end)] = '\0';

        if(debugMode) if (host) printf("Host: %s\n", host);

        arg += strlen(host);
    } else {
        fprintf(stderr, "There is information missing in the arguments given!\n");
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    url_path = strstr(arg, "/") + 1;

    if(debugMode) if (url_path) printf("Url: %s\n", url_path);

    if (strlen(url_path) <= 0) {
        fprintf(stderr, "There is information missing in the arguments given!\n");
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    if ((h = gethostbyname(host)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    ip_addr = inet_ntoa(*((struct in_addr *) h->h_addr_list[0]));

    printf("\nIP Address : %s\n", ip_addr);

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip_addr);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    processResp(sockfd);

    /*send a string to the server*/
    bytes = write(sockfd, buf, strlen(buf));
    if (bytes > 0)
        printf("Bytes escritos %ld\n", bytes);
    else {
        perror("write()");
        exit(-1);
    }

    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }

    // Clean mallocs
    free(user);
    free(password);
    free(host);
    return 0;
}

void processResp(int sockfd) {
    int stillReading = TRUE;
	char c;

    while(stillReading) {
        read(sockfd, &c, 1);
		if(debugMode) printf("%c", c);
    }
}