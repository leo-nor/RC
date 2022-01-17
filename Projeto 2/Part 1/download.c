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
#define ServerPort 21

int debugMode = TRUE;

enum Responses {
    SERVICE_READY,
    SPECIFY_PASSWORD,
    LOGIN_DONE,
    PASSIVE_MODE,
};

static const char * const ResponseCodes[] = {
	[SERVICE_READY] = "220",
	[SPECIFY_PASSWORD] = "331",
	[LOGIN_DONE] = "230",
	[PASSIVE_MODE] = "227",
};

enum AccessCommands {
    USER_NAME,
    PASSWORD,
    ACCOUNT,
    CHANGE_WORKING_DIRECTORY,
    CHANGE_TO_PARENT_DIRECTORY,
    STRUCTURE_MOUNT,
    REINITIALIZE,
    LOGOUT,
};

static const char * const AccessCommands[] = {
	[USER_NAME] = "USER",
	[PASSWORD] = "PASS",
	[ACCOUNT] = "ACCT",
	[CHANGE_WORKING_DIRECTORY] = "CWD",
	[CHANGE_TO_PARENT_DIRECTORY] = "CDUP",
	[STRUCTURE_MOUNT] = "SMNT",
	[REINITIALIZE] = "REIN",
	[LOGOUT] = "QUIT",
};

enum ParameterCommands {
    DATA_PORT,
    PASSIVE,
    REPRESENTATION_TYPE,
    FILE_STRUCTURE,
    TRANSFER_MODE,
};

static const char * const ParameterCommands[] = {
	[DATA_PORT] = "PORT",
	[PASSIVE] = "PASV",
	[REPRESENTATION_TYPE] = "TYPE",
	[FILE_STRUCTURE] = "STRU",
	[TRANSFER_MODE] = "MODE",
};

enum ServiceCommands {
    RETRIEVE,
    STORE,
    STORE_UNIQUE,
    APPEND,
    ALLOCATE,
    RESTART,
    RENAME_FROM,
    RENAME_TO,
    ABORT,
    DELETE,
    REMOVE_DIRECTORY,
    MAKE_DIRECTORY,
    PRINT_WORKING_DIRECTORY,
    LIST,
    NAME_LIST,
    SITE_PARAMETERS,
    SYSTEM,
    STATUS,
    HELP,
    NOOP,
};

static const char * const ServiceCommands[] = {
    [RETRIEVE] = "RETR",
    [STORE] = "STOR",
    [STORE_UNIQUE] = "STOU",
    [APPEND] = "APPE",
    [ALLOCATE] = "ALLO",
    [RESTART] = "REST",
    [RENAME_FROM] = "RNFR",
    [RENAME_TO] = "RNTO",
    [ABORT] = "ABOR",
    [DELETE] = "DELE",
    [REMOVE_DIRECTORY] = "RMD",
    [MAKE_DIRECTORY] = "MKD",
    [PRINT_WORKING_DIRECTORY] = "PWD",
    [LIST] = "LIST",
    [NAME_LIST] = "NLST",
    [SITE_PARAMETERS] = "SITE",
    [SYSTEM] = "SYST",
    [STATUS] = "STAT",
    [HELP] = "HELP",
    [NOOP] = "NOOP",
};

enum Stages {
    ESTABLISH_CONNECTION = 1,
    SEND_USERNAME = 2,
    SEND_PASSWORD = 3,
    GET_TRANSFER_PORT = 4,
};

typedef struct {
    unsigned char *user;
    unsigned char *password;
    unsigned char *url_path;
    unsigned char *host;
    unsigned char *ip_addr;
    int isAnonymous;
    int transferPort;
} FTPTask;

void processResp(int sockfd, FTPTask *task, int stage);
void clearReading(int sockfd);
FTPTask processArgument(char *argv[]);

int main(int argc, char *argv[]) {
    struct hostent *h;
    FTPTask task;

    int sockfd, respsockfd;
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

    task = processArgument(argv);

    if ((h = gethostbyname(task.host)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    task.ip_addr = inet_ntoa(*((struct in_addr *) h->h_addr_list[0]));

    if(debugMode) printf("\nIP Address: %s\n", task.ip_addr);

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(task.ip_addr);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(ServerPort);        /*server TCP port must be network byte ordered */

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

    processResp(sockfd, &task, ESTABLISH_CONNECTION);

    // Login process
    if(!task.isAnonymous) {
        printf(" > Sending Username\n");
        write(sockfd, AccessCommands[USER_NAME], strlen(AccessCommands[USER_NAME]));
        write(sockfd, " ", 1);
        write(sockfd, task.user, strlen(task.user));
        write(sockfd, "\n", 1);
        processResp(sockfd, &task, SEND_USERNAME);

        printf(" > Sending Password\n");
        write(sockfd, AccessCommands[PASSWORD], strlen(AccessCommands[PASSWORD]));
        write(sockfd, " ", 1);
        write(sockfd, task.password, strlen(task.password));
        write(sockfd, "\n", 1);
        processResp(sockfd, &task, SEND_PASSWORD);
    }

    // Request and process data transfer port
    write(sockfd, ParameterCommands[PASSIVE], strlen(ParameterCommands[PASSIVE]));
    write(sockfd, "\n", 1);
    processResp(sockfd, &task, GET_TRANSFER_PORT);

    printf("Transfer port: %i\n", task.transferPort);

    /*server address handling*/
    bzero((char *) &resp_addr, sizeof(resp_addr));
    resp_addr.sin_family = AF_INET;
    resp_addr.sin_addr.s_addr = inet_addr(task.ip_addr);    /*32 bit Internet address network byte ordered*/
    resp_addr.sin_port = htons(task.transferPort);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((respsockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(respsockfd, (struct sockaddr *) &resp_addr, sizeof(resp_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

    // Request and process file
    write(respsockfd, ParameterCommands[RETRIEVE], strlen(ParameterCommands[RETRIEVE]));
    write(respsockfd, "\n", 1);
    processResp(respsockfd, &task, GET_TRANSFER_PORT);

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

    return 0;
}

FTPTask processArgument(char *argv[]) {
    unsigned char *tmp, *end, *host, *url_path, *user, *password = NULL;
    int isAnonymous = TRUE;

    FTPTask task;

    tmp = argv[1];

    if (!strstr(tmp, "ftp://")) {
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    tmp += + strlen("ftp://");

    if (end = strstr(tmp, ":")) {
        task.isAnonymous = FALSE;

        user = (char *) malloc(strlen(tmp) - strlen(end) + 1);
        memcpy(user, tmp, strlen(tmp) - strlen(end));
        user[strlen(tmp) - strlen(end)] = '\0';

        if(debugMode) if (user) printf("User: %s\n", user);

        tmp += + strlen(user) + 1;

        if (end = strstr(tmp, "@")) {
            password = (char *) malloc(strlen(tmp) - strlen(end));
            memcpy(password, tmp, strlen(tmp) - strlen(end));
            password[strlen(tmp) - strlen(end)] = '\0';

            if(debugMode) if (password) printf("Password: %s\n", password);

            tmp += + strlen(password) + 1;
        }
    } else task.isAnonymous = TRUE;

    if (end = strstr(tmp, "/")) {
        host = (char *) malloc(strlen(tmp) - strlen(end) + 1);
        memcpy(host, tmp, strlen(tmp) - strlen(end) + 1);
        host[strlen(tmp) - strlen(end)] = '\0';

        if(debugMode) if (host) printf("Host: %s\n", host);

        tmp += strlen(host);
    } else {
        fprintf(stderr, "There is information missing in the arguments given!\n");
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    url_path = strstr(tmp, "/") + 1;

    if(debugMode) if (url_path) printf("Url: %s\n", url_path);

    if (strlen(url_path) <= 0) {
        fprintf(stderr, "There is information missing in the arguments given!\n");
        fprintf(stderr, "Usage: %s ftp://[<user>:<password>@]<host>/<url-path>\n", argv[0]);
        exit(-1);
    }

    task.host = host;
    task.user = user;
    task.password = password;
    task.url_path = url_path;

    return task;
}

void processResp(int sockfd, FTPTask *task, int stage) {
	char c;
    unsigned char respCode[3] = "", p1[4] = "", p2[4] = "";
    int stillReading = TRUE, hasToProcess = FALSE;
    int ignoreCommas = -4;

    printf("Response: ");
    while(stillReading) {
        read(sockfd, &c, 1);
		if(debugMode) printf("%c", c);

        switch (stage) {
            case ESTABLISH_CONNECTION:
                if(strlen(respCode) < 3) {
                    respCode[strlen(respCode)] = c;
                } else {
                    if(debugMode) printf("Response code: %s\n", respCode);
                    if(!strcmp(respCode, ResponseCodes[SERVICE_READY])) printf("Connection to server established sucessfully\n");
                    else {
                        printf("Connection to server failed!\n");
                        exit(-1);
                    }
                    stillReading = FALSE;
                }
                break;
            case SEND_USERNAME:
                if(strlen(respCode) < 3) {
                    respCode[strlen(respCode)] = c;
                } else {
                    if(debugMode) printf("Response code: %s\n", respCode);
                    if(strcmp(respCode, ResponseCodes[SPECIFY_PASSWORD])) {
                        printf("Failed to receive valid response to username!\n");
                        exit(-1);
                    }
                    stillReading = FALSE;
                }
                break;
            case SEND_PASSWORD:
                if(strlen(respCode) < 3) {
                    respCode[strlen(respCode)] = c;
                } else {
                    if(debugMode) printf("Response code: %s\n", respCode);
                    if(!strcmp(respCode, ResponseCodes[LOGIN_DONE])) printf("Login process sucessful\n");
                    else {
                        printf("Failed to receive valid response to password!\n");
                        exit(-1);
                    }
                    stillReading = FALSE;
                }
                break;
            case GET_TRANSFER_PORT:
                if(!hasToProcess) {
                    if(strlen(respCode) < 3) {
                        respCode[strlen(respCode)] = c;
                    } else {
                        if(debugMode) printf("Response code: %s\n", respCode);
                        if(!strcmp(respCode, ResponseCodes[PASSIVE_MODE])) printf("Entered passive mode\n");
                        else {
                            printf("Failed to receive valid response to passive mode request!\n");
                            exit(-1);
                        }
                        hasToProcess = TRUE;
                    }
                } else {
                    if(c == ')') {
                        task->transferPort = atoi(p1) * 256 + atoi(p2);
                        stillReading = FALSE;
                    } else {
                        if(c == ',')
                            ignoreCommas++;
                        else if(ignoreCommas == 0)
                            p1[strlen(p1)] = c;
                        else if(ignoreCommas == 1)
                            p2[strlen(p2)] = c;
                    }
                }
                break;
            default:
                fprintf(stderr, "Error: Invalid stage given!\n");
                exit(-1);
                break;
        }
    }

    clearReading(sockfd);
}

void clearReading(int sockfd) {
	char c;
    printf("Response: ");
    do {
        read(sockfd, &c, 1);
		if(debugMode) printf("%c", c);
    } while(c != '\n');
}