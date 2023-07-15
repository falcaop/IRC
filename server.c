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
#define MESSAGE_SIZE 4096
#define BUFFER_SIZE HEADER_SIZE+MESSAGE_SIZE

// definição da estrutura de um cliente
struct client{
    int socket;
    pthread_t thread;
    char nickname[50];
    char muted;
    struct channel *channel;
    struct sockaddr_in address;
};

// definição da estrutura de um canal
struct channel{
    char name[201];
    struct client *admin;
};

int server_socket;
int client_count, channel_count = 0;
struct client clients[MAX_CLIENTS];
struct channel channels[MAX_CHANNELS];
pthread_mutex_t client_mutex;

// finaliza o programa
void exit_server();

// finaliza a conexão com um cliente
// fecha o seu socket, finaliza sua thread, remove da lista de clientes conectados
void close_socket(int socket);

// envia a mensagem em broadcast para todos os clientes conectados no canal.
void broadcast(char *message, struct channel *channel);

// retorna o endereço do cliente com um determinado nickname em um determinado canal, ou NULL, se não existir
struct client *find_client(char *nickname, struct channel *channel);

// função executada pela thread que recebe mensagens de um cliente e envia para todos os clientes no canal
void handle_client(void *client);

// função principal
int main() {
    signal(SIGINT, exit_server);

    int client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_length;

    // inicializar mutex, usado para sincronizar o acesso das threads às variáveis globais
    if (pthread_mutex_init(&client_mutex, NULL) != 0) {
        printf("Erro ao inicializar o mutex.\n");
        return 1;
    }

    // criar socket do servidor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        printf("Erro ao criar o socket do servidor.\n");
        return 1;
    }

    // configurar endereço do servidor
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(12345);

    // vincular o socket à porta
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("Erro ao vincular o socket à porta.\n");
        return 1;
    }

    // escutar por conexões
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Erro ao escutar por conexões");
        exit(EXIT_FAILURE);
    }

    printf("Servidor iniciado. Aperte CTRL + C para encerrar.\nAguardando conexões de clientes...\n");

    // aceitação de conexões com os clientes
    while (1) {
        // aceitar a conexão
        client_address_length = sizeof(client_address);
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_length);
        if (client_socket < 0) {
            printf("Erro ao aceitar a conexão do cliente.\n");
            continue;
        }

        // verificar se atingiu número máximo de clientes
        if (client_count >= MAX_CLIENTS) {
            printf("Número máximo de clientes atingido. Rejeitando nova conexão.\n");
            close(client_socket);
            continue;
        }

        // adicionar cliente à lista de clientes conectados
        pthread_mutex_lock(&client_mutex);
        clients[client_count].socket = client_socket;
        clients[client_count].nickname[0] = '\0';
        clients[client_count].channel = NULL;
        clients[client_count].address = client_address;
        client_count++;
        pthread_mutex_unlock(&client_mutex);

        // criar thread para executar o recebimento e envio de mensagens para esse cliente
        pthread_create(&clients[client_count - 1].thread, NULL, (void *)handle_client, &clients[client_count-1]);

        printf("Novo cliente conectado. Socket: %d, IP: %s, Porta: %d\n",
            client_socket,
            inet_ntoa(client_address.sin_addr),
            ntohs(client_address.sin_port));
    }

    return 0;
}

void exit_server(){
    printf("\nEncerrando o servidor...\n");
    pthread_mutex_destroy(&client_mutex);
    close(server_socket);
    exit(0);
}

void close_socket(int socket){
    pthread_t thread;
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == socket) {
            thread = clients[i].thread;
            clients[i].thread = 0;
            clients[i] = clients[client_count - 1];
            break;
        }
    }
    client_count--;
    pthread_mutex_unlock(&client_mutex);
    close(socket);
    pthread_cancel(thread);
}

void broadcast(char *message, struct channel *channel) {
    int pending_close[MAX_CLIENTS];
    int pending_close_index = 0;
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++) {
        if(channel == clients[i].channel){
            int retries = 0;
            // caso não seja enviado, tenta reenviar até 5 vezes
            while((retries++ < 5) && (send(clients[i].socket, message, strlen(message) + 1, 0) < 1));
            if(retries == 6) pending_close[pending_close_index++] = clients[i].socket;
        }
    }
    pthread_mutex_unlock(&client_mutex);
    // fechar conexão com usuários que não recebem mensagens
    for(int i = 0; i < pending_close_index; i++) close_socket(pending_close[pending_close_index]);
}

struct client *find_client(char *nickname, struct channel *channel){
    struct client *client = NULL;
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < client_count; i++){
        if ((clients[i].channel == channel) && (strcmp(nickname, clients[i].nickname) == 0)){
            client = &clients[i];
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
    return client;
}

void handle_client(void *client) {
    struct client *current_client = (struct client *)client; 
    char buffer[BUFFER_SIZE];

    // recebimento da mensagem
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int read_size = recv(current_client->socket, buffer, BUFFER_SIZE, 0);
        if (read_size <= 0) {
            close_socket(current_client->socket);
            break;
        } 
        // comandos para todos os clientes
        // /ping, retornar pong para o cliente
        if (strcmp(buffer, "/ping") == 0){
            send(current_client->socket, "pong", 5, 0);
            continue;
        }
        // alterar apelido
        if (strncmp(buffer, "/nickname ", 10) == 0){
            strncpy(current_client->nickname, &buffer[10], strlen(buffer)-10);
            current_client->nickname[strlen(buffer)-10] = '\0';

            char msg[100];
            sprintf(msg, "Apelido atualizado para: %s", current_client->nickname);
            send(current_client->socket, msg, strlen(msg) + 1, 0);
            continue;
        }
        // verificar se o cliente tem apelido para poder enviar mensagens
        if(current_client->nickname[0] == '\0'){
            send(current_client->socket, "Digite '/nickname (apelido)' para definir o seu apelido", 56, 0);
            continue;
        }
        // entrar em um canal
        if (strncmp(buffer, "/join ", 6) == 0){    
            if((strlen(buffer) - 6) > 200){
                send(current_client->socket, "Nomes de canais não podem ser maiores que 200 caracteres", 57, 0);
                continue;
            }

            char channelName[201];
            strcpy(channelName, &buffer[6]);
            // se o canal existir, adicionar cliente ao canal
            int exists = 0;
            for (int i = 0; i < channel_count; i++){
                if (strcmp(channels[i].name, channelName) == 0){
                    current_client->channel = &channels[i];
                    exists = 1;
                    break;
                }
            }
            // se canal não existir, criar novo canal
            if (!exists){
                // verificar se é um nome de canal válido
                if((channelName[0] != '&') && (channelName[0] != '#')){
                    send(current_client->socket, "Nomes de canais devem começar com '&' ou '#'", 45, 0);
                    continue;
                }
                if(strchr(channelName, ' ')){
                    send(current_client->socket, "Nomes de canais não podem conter espaços", 41, 0);
                    continue;
                }
                if(strchr(channelName, 7)){
                    send(current_client->socket, "Nomes de canais não podem conter control G", 43, 0);
                    continue;
                }
                if(strchr(channelName, ',')){
                    send(current_client->socket, "Nomes de canais não podem conter virgulas", 42, 0);
                    continue;
                }
                // criar o canal e definir o cliente como administrador
                strcpy(channels[channel_count].name, channelName);
                channels[channel_count].admin = current_client;
                current_client->channel = &channels[channel_count];
                channel_count++;
            }

            char msg[230];
            sprintf(msg, "Você se juntou ao canal: %s", channelName);
            send(current_client->socket, msg, strlen(msg) + 1, 0);
            continue;
        }
        // verificar se o cliente está em um canal para poder enviar mensagens
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
            // expulsar um cliente do canal
            if (strncmp(buffer, "/kick ", 6) == 0){
                char nickname[50];
                strncpy(nickname, &buffer[6], strlen(buffer)-6);
                nickname[strlen(buffer)-6] = '\0';

                // verificar se o cliente existe e não é administrador
                struct client *client = find_client(nickname, current_client->channel);
                if (!client){
                    char *msg = "Usuário não encontrado";
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                    continue;
                }
                if (client == current_client){
                    char *msg = "Não é possível usar a operação em administradores";
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                    continue;
                }

                // enviar mensagens de confirmação e finalizar conexão do cliente
                char msg[100];
                sprintf(msg, "O usuário %s foi expulso do canal", client->nickname);
                send(current_client->socket, msg, strlen(msg) + 1, 0);
                sprintf(msg, "Você foi expulso do canal");
                send(client->socket, msg, strlen(msg) + 1, 0);
                close_socket(client->socket);

                continue;
            }
            // silenciar um cliente do canal
            if (strncmp(buffer, "/mute ", 6) == 0 && current_client->channel->admin == current_client){
                char nickname[50];
                strncpy(nickname, &buffer[6], strlen(buffer)-6);
                nickname[strlen(buffer)-6] = '\0';

                // verificar se o cliente existe e não é administrador
                struct client *client = find_client(nickname, current_client->channel);
                if (!client){
                    char *msg = "Usuário não encontrado";
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                    continue;
                }
                if (client == current_client){
                    char *msg = "Não é possível usar a operação em administradores";
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                    continue;
                }

                // enviar mensagens de confirmação e silenciar cliente
                client->muted = 1;
                char msg[100];
                sprintf(msg, "O usuário %s foi silenciado", client->nickname);
                send(current_client->socket, msg, strlen(msg) + 1, 0);
                sprintf(msg, "Você foi silenciado");
                send(client->socket, msg, strlen(msg) + 1, 0);

                continue;
            }
            // retirar o mute de um usuário
            if (strncmp(buffer, "/unmute ", 8) == 0){
                char nickname[50];
                strncpy(nickname, &buffer[8], strlen(buffer)-8);
                nickname[strlen(buffer)-8] = '\0'; 
                
                // verificar se o cliente existe e não é administrador
                struct client *client = find_client(nickname, current_client->channel);
                if (!client){
                    char *msg = "Usuário não encontrado";
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                    continue;
                }
                if (client == current_client){
                    char *msg = "Não é possível usar a operação em administradores";
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                    continue;
                }

                // enviar mensagens de confirmação e retirar o mute do cliente
                client->muted = 0;
                char msg[100];
                sprintf(msg, "O usuário %s não está mais silenciado", client->nickname);
                send(current_client->socket, msg, strlen(msg) + 1, 0);
                sprintf(msg, "Você não está mais silenciado");
                send(client->socket, msg, strlen(msg) + 1, 0);

                continue;
            }
            // solicitar IP de um cliente
            if (strncmp(buffer, "/whois ", 7) == 0){
                char nickname[50];
                strncpy(nickname, &buffer[7], strlen(buffer)-7);
                nickname[strlen(buffer)-7] = '\0';

                // verificar se o cliente existe e não é administrador
                struct client *client = find_client(nickname, current_client->channel);
                if (!client){
                    char *msg = "Usuário não encontrado";
                    send(current_client->socket, msg, strlen(msg) + 1, 0);
                    continue;
                }

                // enviar IP do cliente solicitado
                char msg[100];
                sprintf(msg, "IP do usuário %s: %s", client->nickname, inet_ntoa(client->address.sin_addr));
                send(current_client->socket, msg, strlen(msg) + 1, 0);

                continue;
            }
        }
        // verificar se o cliente pode enviar mensagens
        if (current_client->muted){
            char *msg = "Você está silenciado";
            send(current_client->socket, msg, strlen(msg) + 1, 0);
            continue;
        }

        // mensagem comum: adicionar apelido do cliente no inicio e enviar para todos do canal
        char message[HEADER_SIZE + MESSAGE_SIZE];
        strcpy(message, current_client->nickname);
        strcat(message, ": ");
        strcat(message, buffer);
        broadcast(message, current_client->channel);
    }
}