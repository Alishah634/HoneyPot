#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORTS {22, 21, 80}
#define MAX_CLIENTS 5  // Max simultaneous connections

#define LOG_FILE "honeypot.log"
#define MAX_BUFFER_SIZE 1024

void *start_server(void *port_ptr);

void log_login_attempt(const char *service, const char *ip, const char *username, const char *password) {
    FILE *log = fopen(LOG_FILE, "a");
    if (log == NULL) {
        perror("Failed to open log file");
        return;
    }

    time_t now = time(NULL);
    fprintf(log, "[%s] %s - IP: %s | Username: %s | Password: %s\n", 
            ctime(&now), service, ip, username ? username : "N/A", password ? password : "N/A");
    
    fclose(log);
}

void log_execution(const char service[], const char client_ip[], const char command[])
{
    FILE* log = fopen(LOG_FILE, "a");
    if(!log){
        perror("Failed to open log file");
        return;
    }
    time_t now = time(NULL);
    fprintf(log, "[%s] %s Command Executed: %s",ctime(&now), service, command);
    fclose(log);
    return; 
}

void handle_fake_ssh(int client_socket, const char *client_ip) {
    char banner[] = "SSH-2.0-OpenSSH_8.4p1 Debian-5ubuntu1\r\n";
    send(client_socket, banner, strlen(banner), 0);

    // Initialize as empty string
    char buffer[MAX_BUFFER_SIZE] = {0}; 
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received > 0) {
        // If client sends an encrypted SSH message, disconnect them, forces them to use netcat or telnet to connect. ssh will fail
        if ((unsigned char)buffer[0] == 0x20 || (unsigned char)buffer[0] == 0x14) {
            printf("[SSH] %s attempted an encrypted connection. Sending rejection.\n", client_ip);

            // Send a proper SSH rejection message
            char rejection_msg[] = "SSH-2.0-OpenSSH_8.4p1 Debian-5ubuntu1\r\n"
            "Protocol mismatch.\r\n";
            send(client_socket, rejection_msg, strlen(rejection_msg), 0);

            char fake_rejection_message[] = "ssh: connect to host 172.26.153.3 port 22: Connection refused\r\n";
            send(client_socket, fake_rejection_message, strlen(fake_rejection_message), 0);

            close(client_socket);
            return;
        }
        printf("[SSH] %s tried SSH connection. Disconnecting.\n", client_ip);
        // char fake_rejection_message[] = "Use netcat\r\n";
        // send(client_socket, fake_rejection_message, strlen(fake_rejection_message), 0);
    }

    char username_prompt[] = "Username: ";
    send(client_socket, username_prompt, strlen(username_prompt), 0);

    // Initialize as empty string
    char username[MAX_BUFFER_SIZE] = {0}; 
    recv(client_socket, username, sizeof(username) - 1, 0);
    // Replace with end of string character
    username[strcspn(username, "\r\n")] = 0; 

    char password_prompt[] = "Password: ";
    send(client_socket, password_prompt, strlen(password_prompt), 0);

    // Initialize as empty string
    char password[MAX_BUFFER_SIZE] = {0}; 
    recv(client_socket, password, sizeof(password) - 1, 0);
    // Replace with end of string character
    password[strcspn(password, "\r\n")] = 0; 
    log_login_attempt("SSH", client_ip, username, password);
    
    // Fake login success message
    char login_success[] = "Welcome to Ubuntu 20.04.3 LTS (GNU/Linux 5.11.0-43-generic x86_64)\r\n";
    send(client_socket, login_success, strlen(login_success), 0);

    // Fake shell prompt
    // char shell_prompt[] = "user@honeypot:~$ ";
    char shell_prompt[MAX_BUFFER_SIZE] = "user@LAPTOP-2N3BCM63:~$ ";
    if (strcmp(username, "root") == 0) {
        printf("Switched user to root!!!\n");
        strncpy(shell_prompt, "root@LAPTOP-2N3BCM63:~$ ", sizeof(shell_prompt) - 1);
        shell_prompt[sizeof(shell_prompt) - 1] = '\0';
        // strcpy(shell_prompt, "root@LAPTOP-2N3BCM63:~$ "); // Learning Experience!!!
    }
    
    send(client_socket, shell_prompt, strlen(shell_prompt), 0);

    // Fake shell commands
    // char buffer[1024];
    while (recv(client_socket, buffer, sizeof(buffer) - 1, 0) > 0) {
      // Remove newline characters
        buffer[strcspn(buffer, "\r\n")] = 0;  
        printf("[SSH] %s executed: %s\n", client_ip, buffer);

        log_execution("[SSH]", client_ip, buffer);

        if (strcmp(buffer, "exit") == 0) {
            char exit_message[] = "Connection closed by remote host.\r\n";
            send(client_socket, exit_message, strlen(exit_message), 0);
            break;
        } else if (strcmp(buffer, "ls") == 0) {
            char fake_ls[] = "Desktop  Documents  Downloads  Pictures  Videos\r\n";
            send(client_socket, fake_ls, strlen(fake_ls), 0);
        } else if (strcmp(buffer, "whoami") == 0) {
            char fake_whoami[MAX_BUFFER_SIZE];

            if(strcmp(username, "root")==0) {
                strncpy(fake_whoami, "root\r\n", sizeof(fake_whoami));
                
            } else {
                strncpy(fake_whoami, "user\r\n", sizeof(fake_whoami));
            }

            send(client_socket, fake_whoami, strlen(fake_whoami), 0);
        }  else if (strcmp(buffer, "clear") == 0) {
            // Send ANSI escape sequence to clear the screen
            char clear_screen[] = "\033[H\033[J";
            send(client_socket, clear_screen, strlen(clear_screen), 0);
        } else {
            char unknown_command[] = "bash: command not found\r\n";
            send(client_socket, unknown_command, strlen(unknown_command), 0);
        }

        send(client_socket, shell_prompt, strlen(shell_prompt), 0);
    }

    close(client_socket);
}

void handle_fake_ftp(int client_socket, const char *client_ip) {
    char banner[] = "220 Welcome to FTP server (vsFTPd 3.0.3)\r\n";
    send(client_socket, banner, strlen(banner), 0);

    char user_prompt[] = "331 Please specify the password.\r\n";
    send(client_socket, user_prompt, strlen(user_prompt), 0);

    // Initialize as empty string
    char user[MAX_BUFFER_SIZE] = {0}; 
    recv(client_socket, user, sizeof(user) - 1, 0);
    user[strcspn(user, "\r\n")] = 0;

    char pass_prompt[] = "230 Login successful.\r\n";
    send(client_socket, pass_prompt, strlen(pass_prompt), 0);

    log_login_attempt("FTP", client_ip, user, NULL);
    
    char buffer[MAX_BUFFER_SIZE];
    while (recv(client_socket, buffer, sizeof(buffer) - 1, 0) > 0) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        fprintf("[FTP] %s executed: %s\n", client_ip, buffer);
        
        if (strcmp(buffer, "ls") == 0) {
            char fake_ls[] = "pub  private  upload  readme.txt\r\n";
            send(client_socket, fake_ls, strlen(fake_ls), 0);
        } else if (strcmp(buffer, "get readme.txt") == 0) {
            char fake_file[] = "This is a test file.\r\n";
            send(client_socket, fake_file, strlen(fake_file), 0);
        } else {
            char unknown_command[] = "500 Unknown command.\r\n";
            send(client_socket, unknown_command, strlen(unknown_command), 0);
        }
    }
    close(client_socket);
}

void handle_fake_http(int client_socket, const char *client_ip) {
    // Initialize as empty string
    char request[MAX_BUFFER_SIZE] = {0}; 
    recv(client_socket, request, sizeof(request) - 1, 0);
    
    printf("[HTTP] %s requested: %s\n", client_ip, request);
    // log_login_attempt("HTTP", client_ip, request, NULL);
    
    
    if (strstr(request, "GET /admin")) {
        char http_response[] =
        "HTTP/1.1 401 Unauthorized\r\n"
        "Content-Length: 0\r\n"
        "WWW-Authenticate: Basic realm=\"Restricted Area\"\r\n"
        "\r\n";
        send(client_socket, http_response, strlen(http_response), 0);
    } else {
        char http_response[] =
        "HTTP/1.1 200 OK\r\n"
        "Server: Apache\r\n"
        "Content-Length: 13\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Welcome Page!";
        send(client_socket, http_response, strlen(http_response), 0);
    }

    close(client_socket);
}

void handle_connection(int client_socket, struct sockaddr_in client_addr, int port) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    printf("Connection attempt from %s on port %d\n", client_ip, port);
    if (port == 22) {  
        handle_fake_ssh(client_socket, client_ip); 
    } else if (port == 21) {  
        handle_fake_ftp(client_socket, client_ip);
    } else if (port == 80) {  
        handle_fake_http(client_socket, client_ip);
    } else {
      fprintf(stderr, "No connection...");
    }

    close(client_socket);
}

int main() {
    int ports[] = PORTS;
    int num_ports = sizeof(ports) / sizeof(ports[0]);
    pthread_t threads[num_ports];

    for (int i = 0; i < num_ports; i++) {
        int *port = malloc(sizeof(int));
        *port = ports[i];
        pthread_create(&threads[i], NULL, start_server, port);
      // Prevents simultaneous binding issues
        sleep(1);  
    }

    for (int i = 0; i < num_ports; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

void *start_server(void *port_ptr) {
    int port = *(int *)port_ptr;
    free(port_ptr);
    
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return NULL;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;   
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return NULL;
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        return NULL;
    }

   printf("Honeypot listening on port %d...\n", port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }
        handle_connection(client_socket, client_addr, port);
    }

    close(server_socket);
    return NULL;
}
