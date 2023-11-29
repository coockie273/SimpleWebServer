#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>

#define DAEMON_NAME "webserver"
#define WORKING_DIRECTORY  "/"
#define STORAGE_PATH "/home/coockie273/pages"
#define PORT 8080

int listener;

void daemonize() {

    pid_t pid;
    pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "Forking failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    pid_t sid = setsid();

    if (sid < 0) {
        syslog(LOG_ERR, "Failed to create SID");
        exit(EXIT_FAILURE);
    }

    if (chdir(WORKING_DIRECTORY) < 0) {
        syslog(LOG_ERR, "Failed to change the working directory");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

}


void signal_handler(int sig) {

    switch(sig) {

        case SIGTERM:
            syslog(LOG_INFO, "Web server was terminated");
            close(listener);
            closelog();
            exit(0);
            break;

    }

}


void send_file(int sock, const char* file_path, const char* dot, const char* type) {

    char buf[1024];

    FILE *file = fopen(file_path, "rb");

    if (! file) {

        syslog(LOG_ERR, "Failed to open file");
        return;

    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    sprintf(buf, "HTTP/1.1 200 OK\nContent-Type: %s/%s\nContent-Length: %ld\n\n", type, dot, file_size);
    send(sock, buf, strlen(buf), 0);

    char* response = (char*)malloc(file_size);

    fread(response, 1, file_size, file);

    int bytes_sended = send(sock, response, file_size, 0);

    syslog(LOG_INFO, "Data from the %s size %d was successfully sent to the clent", file_path, bytes_sended); 

    fclose(file);

    free(response);



}

void parse_request(int sock) {

    char buf[1024];
    recv(sock, buf, sizeof(buf), 0);

    char* url_path = strstr(buf, "GET");


    if (url_path != NULL) {

        url_path += 5;
        char* end_url_path = strchr(url_path, ' ');

        if (end_url_path != NULL) {

            *end_url_path = '\0';

            char file_path[256];

            sprintf(file_path, "%s/%s", STORAGE_PATH, url_path);

            const char* dot = strrchr(file_path,'.');
            dot += 1;

            char* type;

            if (strcmp(dot,"img") == 0 || strcmp(dot, "jpg") == 0 || strcmp(dot,"jpeg") == 0) {

                type = "image";

            } else {

                type = "text";
            }

            send_file(sock, file_path, dot, type);
        }

    }

}

void next_client(int listener) {

    int sock = accept(listener, NULL, NULL);

    if (sock < 0) {

        syslog(LOG_ERR, "Failed to accept client");
        return;
    }

    switch(fork()) {

        case -1:

            syslog(LOG_ERR, "Failed to fork new client");
            break;

        case 0:

            close(listener);

            parse_request(sock);

            close(sock);
            _exit(0);
        default:
            close(sock);
    }
}


int main() {

    openlog(DAEMON_NAME, LOG_PID, LOG_USER);

    signal(SIGTERM, signal_handler);

    daemonize();

    struct sockaddr_in addr;

    listener = socket(AF_INET, SOCK_STREAM, 0);

    if (listener < 0) {
        syslog(LOG_ERR, "Failed to create listener socket");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0) {

       syslog(LOG_ERR, "Failed to bind listener");
       exit(EXIT_FAILURE);

    }

    syslog(LOG_INFO, "Web server was started");
    listen(listener, 10);

    while(1) {

        next_client(listener);

    }


}
