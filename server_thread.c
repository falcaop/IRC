#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define MAX_CLIENTS 10
#define MAX_CHANNELS 10
#define HEADER_SIZE 52
#define MESSAGE_SIZE 64
#define BUFFER_SIZE HEADER_SIZE+MESSAGE_SIZE

struct client{
    int socket;
    pthread_t thread; // talvez nao precise
    char nickname[50];
    char muted;
    struct channel *channel;
    struct sockaddr_in address;
};

struct channel{
    char name[50];
    struct client *admin;
};

int client_count, channel_count = 0;
struct client clients[MAX_CLIENTS];
struct channel channels[MAX_CHANNELS];
pthread_mutex_t client_mutex;

void exit_server(){
    printf("\nEncerrando o servidor...\n");
    pthread_mutex_destroy(&client_mutex);
    exit(0);
}

void broadcast(char *message, struct channel *channel) {
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        if (channel == clients[i].channel)
            send(clients[i].socket, message, strlen(message) + 1, 0);
    }
    pthread_mutex_unlock(&client_mutex);
}

void close_socket(int socket){
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == socket) {
            clients[i] = clients[client_count - 1];
            break;
        }
    }
    client_count--;
    pthread_mutex_unlock(&client_mutex);
    shutdown(socket, SHUT_RD);
    close(socket);
    pthread_exit(NULL);
}

struct client *find_client(char *nickname, struct channel *channel){
    for (int i = 0; i < client_count; i++){
        if ((clients[i].channel == channel) && (strcmp(nickname, clients[i].nickname) == 0)) return &clients[i];
    }
    return NULL;
}

void handle_client(void *client) {
    struct client *current_client = (struct client *)client; 
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int read_size = recv(current_client->socket, buffer, BUFFER_SIZE, 0);
        if (read_size <= 0) {
            printf("hora de fechar\n");
            close_socket(current_client->socket);
            break;
        } 
        // comandos para todos os usuarios
        if (strcmp(buffer, "/ping") == 0){
            send(current_client->socket, "pong", 5, 0);
            continue;
        }
        if (strncmp(buffer, "/nickname ", 10) == 0){
            strncpy(current_client->nickname, &buffer[10], strlen(buffer)-10);
            current_client->nickname[strlen(buffer)-10] = '\0'; //talvez tenha que limpar memoria

            char msg[100];
            sprintf(msg, "Apelido atualizado para: %s", current_client->nickname);
            send(current_client->socket, msg, strlen(msg) + 1, 0);
            continue;
        }
        if(current_client->nickname[0] == '\0'){
            send(current_client->socket, "Digite '/nickname (apelido)' para definir o seu apelido", 56, 0);
            continue;
        }
        if (strncmp(buffer, "/join ", 6) == 0){
            char channelName[50];
            strncpy(channelName, &buffer[6], strlen(buffer)-6);
            channelName[strlen(buffer)-6] = '\0'; //talvez tenha que limpar memoria

            int exists = 0;
            for (int i = 0; i < channel_count; i++){
                if (strcmp(channels[i].name, channelName) == 0){
                    current_client->channel = &channels[i];
                    exists = 1;
                    break;
                }
            }
            if (!exists){
                strcpy(channels[channel_count].name, channelName);
                channels[channel_count].admin = current_client;
                current_client->channel = &channels[channel_count];
                channel_count++;
            }

            char msg[100];
            sprintf(msg, "Você se juntou ao canal: %s", channelName);
            send(current_client->socket, msg, strlen(msg) + 1, 0);
            continue;
        }
        if(current_client->channel == NULL){
            send(
                current_client->socket,
                "Digite '/join (canal)' para se juntar a um canal existinte ou criar um novo",
                76,
                0
            );
            continue;
        }
        // comandos para administradores
        if(current_client->channel->admin == current_client){
            if (strncmp(buffer, "/kick ", 6) == 0){
                char nickname[50];
                strncpy(nickname, &buffer[6], strlen(buffer)-6);
                nickname[strlen(buffer)-6] = '\0'; //talvez tenha que limpar memoria

            struct client *client = find_client(nickname);
            if (client){
                char msg[100];
                sprintf(msg, "O usuário %s foi expulso do canal", client->nickname);
                send(current_client->socket, msg, strlen(msg) + 1, 0);
                sprintf(msg, "Você foi expulso do canal");
                send(client->socket, msg, strlen(msg) + 1, 0);
                close_socket(client->socket);
            }
            else{
                char *msg = "Usuário não encontrado";
                send(current_client->socket, msg, strlen(msg) + 1, 0);
            }
            continue;
        }
        if (strncmp(buffer, "/mute ", 6) == 0 && current_client->channel->admin == current_client){
            char nickname[50];
            strncpy(nickname, &buffer[6], strlen(buffer)-6);
            nickname[strlen(buffer)-6] = '\0'; //talvez tenha que limpar memoria

                struct client *client = find_client(nickname, current_client->channel);
                if (client){
                    client->muted = 1;
                    char msg[100];
                    sprintf(msg, "O usuário %s foi silenciado", client->nickname);
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                    sprintf(msg, "Você foi silenciado");
                    send(client->socket, msg, strlen(msg) + 1, 0);
                }
                else{
                    char *msg = "Usuário não encontrado";
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                }
                continue;
            }
            if (strncmp(buffer, "/unmute ", 8) == 0){
                char nickname[50];
                strncpy(nickname, &buffer[8], strlen(buffer)-8);
                nickname[strlen(buffer)-8] = '\0'; //talvez tenha que limpar memoria
                
                struct client *client = find_client(nickname, current_client->channel);
                if (client){
                    client->muted = 0;
                    char msg[100];
                    sprintf(msg, "O usuário %s não está mais silenciado", client->nickname);
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                    sprintf(msg, "Você não está mais silenciado");
                    send(client->socket, msg, strlen(msg) + 1, 0);
                }
                else{
                    char *msg = "Usuário não encontrado";
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                }
                continue;
            }
            if (strncmp(buffer, "/whois ", 7) == 0){
                char nickname[50];
                strncpy(nickname, &buffer[7], strlen(buffer)-7);
                nickname[strlen(buffer)-7] = '\0'; //talvez tenha que limpar memoria

                struct client *client = find_client(nickname, current_client->channel);
                if (client){
                    char msg[100];
                    sprintf(msg, "IP do usuário %s: %s", client->nickname, inet_ntoa(client->address.sin_addr));
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                }
                else{
                    char *msg = "Usuário não encontrado";
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                }
                continue;
            }
        }
        if (current_client->muted){
            char *msg = "Você está silenciado";
            send(current_client->socket, msg, strlen(msg) + 1, 0);
            continue;
        }
        // mensagem normal
            // colocar apelido
        char message[HEADER_SIZE + MESSAGE_SIZE];
        strcpy(message, current_client->nickname);
        strcat(message, ": ");
        strcat(message, buffer);

        // enviar
        broadcast(message, current_client->channel);
    }
}

int main() {
    signal(SIGINT, exit_server);

    strcpy(channels[0].name, "canal1");
    channel_count = 1;

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
        clients[client_count].socket = client_socket;
        clients[client_count].nickname[0] = '\0';
        clients[client_count].channel = NULL;
        clients[client_count].address = client_address;
        client_count++;
        pthread_mutex_unlock(&client_mutex);

        // Criar thread para lidar com o cliente
        pthread_create(&clients[client_count - 1].thread, NULL, (void *)handle_client, &clients[client_count-1]);

        printf("Novo cliente conectado. Socket: %d, IP: %s, Porta: %d\n",
            client_socket,
            inet_ntoa(client_address.sin_addr),
            ntohs(client_address.sin_port));
            
    }

    return 0;
}
