#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_CLIENTS 10
#define HEADER_SIZE 52
#define MESSAGE_SIZE 32
#define BUFFER_SIZE HEADER_SIZE+MESSAGE_SIZE

int client_count = 0;
// usar uma struct dps?
int client_sockets[MAX_CLIENTS];
char client_nicknames[MAX_CLIENTS][20];
pthread_t client_threads[MAX_CLIENTS];
pthread_mutex_t client_mutex;

void broadcast(char *message, int current_client) {
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        send(client_sockets[i], message, strlen(message), 0);
    }
    pthread_mutex_unlock(&client_mutex);
}

void close_socket(int sock){
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        if (client_sockets[i] == sock) {
            client_sockets[i] = client_sockets[client_count - 1];
            strcpy(client_nicknames[client_count - 1], client_nicknames[i]);
            break;
        }
    }
    client_count--;
    pthread_mutex_unlock(&client_mutex);
    close(sock);
    pthread_exit(NULL);
}

void handle_client(void *client_socket) {
    int sock = *(int *)client_socket;
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int read_size = recv(sock, buffer, BUFFER_SIZE, 0);
        if (read_size <= 0) {
            close_socket(sock);
        } else {
            if (strcmp(buffer, "/ping") == 0){
                send(sock, "pong", 6, 0);
            }
            else if (strncmp(buffer, "/nickname ", 10) == 0){
                printf("buffer: '%s'\n", buffer);
                for (int i = 0; i < client_count; i++){
                    if (client_sockets[i] == sock){
                        char newNickname[20];
                        strncpy(newNickname, &buffer[10], strlen(buffer) - 10);
                        newNickname[strlen(buffer) - 10] = '\0'; 
                        printf("novo apelido: %s\n", newNickname);
                        strcpy(client_nicknames[i], newNickname);
                        break;
                    }
                }
            }
            else if (strncmp(buffer, "/kick ", 6) == 0){
                printf("buffer: '%s'\n", buffer);
                for (int i = 0; i < client_count; i++){
                    if (client_sockets[i] == sock){
                        char nickname[20];
                        strncpy(nickname, &buffer[6], strlen(buffer) - 6);
                        nickname[strlen(buffer) - 6] = '\0'; 
                        printf("apelido lido: '%s'\n", nickname);
                        break;
                    }
                }
            }
            else{   
                // colocar apelido
                char message[HEADER_SIZE + MESSAGE_SIZE];
                for (int i = 0; i < client_count; i++){
                    if (client_sockets[i] == sock){
                        strcpy(message, client_nicknames[i]);
                        break;
                    }
                }
                
                strcat(message, ": ");
                strcat(message, buffer);

                // enviar
                broadcast(message, sock);
            }
        }
    }
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_length;

    // Inicializar mutex
    if (pthread_mutex_init(&client_mutex, NULL) != 0) {
        printf("Falha ao inicializar o mutex.\n");
        return 1;
    }

    // Criar socket do servidor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        printf("Falha ao criar o socket do servidor.\n");
        return 1;
    }

    // Configurar endereço do servidor
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(12345);

    // Vincular o socket à porta
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("Falha ao vincular o socket à porta.\n");
        return 1;
    }

    // Escuta por conexões
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Erro ao escutar por conexões");
        exit(EXIT_FAILURE);
    }

    printf("Aguardando conexões de clientes...\n");

    while (1) {
        // Aceitar conexão de cliente
        client_address_length = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket < 0) {
            printf("Falha ao aceitar a conexão do cliente.\n");
            continue;
        }

        // Verificar se atingiu o número máximo de clientes
        if (client_count >= MAX_CLIENTS) {
            printf("Número máximo de clientes atingido. Rejeitando nova conexão.\n");
            close(client_socket);
            continue;
        }

        // Adicionar cliente à lista
        pthread_mutex_lock(&client_mutex);
        client_sockets[client_count] = client_socket;
        strcpy(client_nicknames[client_count], "teste");
        printf("apelido definido: %s\n", client_nicknames[client_count]);
        client_count++;
        pthread_mutex_unlock(&client_mutex);

        // Criar thread para lidar com o cliente
        pthread_create(&client_threads[client_count - 1], NULL, (void *)handle_client, &client_socket);

        printf("Novo cliente conectado. Socket: %d, IP: %s, Porta: %d\n",
            client_socket,
            inet_ntoa(client_address.sin_addr),
            ntohs(client_address.sin_port));
    }

    // Fechar sockets e liberar recursos
    close(server_socket);
    pthread_mutex_destroy(&client_mutex);

    return 0;
}
