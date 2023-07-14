#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define HEADER_SIZE 52
#define MESSAGE_SIZE 64
#define BUFFER_SIZE HEADER_SIZE+MESSAGE_SIZE

int client_socket;
pthread_t receive_thread;

void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int read_size = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (read_size <= 0) {
            printf("Desconectado do servidor.\n");
            close(client_socket);
            exit(0);
        }
        printf("%s\n", buffer);
        fflush(stdout);
    }
    pthread_exit(NULL);
}

int main() {
    struct sockaddr_in server_address;
    char server_ip[] = "127.0.0.1";
    int server_port = 12345;
    char buffer[BUFFER_SIZE];

    // Criar socket do cliente
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        printf("Falha ao criar o socket do cliente.\n");
        return 1;
    }

    // Configurar endereço do servidor
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(server_ip);
    server_address.sin_port = htons(server_port);

    char *command;
    while(1){
        printf("> ");
        scanf("%ms", &command);
        getchar();
        if(!strcmp(command, "/connect")) break;
        printf("Digite '/connect' para se conectar ao servidor.\n");
    }

    // Conectar ao servidor
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("Falha ao conectar-se ao servidor.\n");
        return 1;
    }

    printf("Conectado ao servidor IRC. Digite '/quit' para encerrar.\n");

    // Iniciar a thread de recebimento de mensagens
    if (pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        printf("Falha ao criar a thread de recebimento de mensagens.\n");
        return 1;
    }

    while (1) {
        // Limpar o buffer
        memset(buffer, 0, BUFFER_SIZE);

        // Ler mensagem do usuário
        fgets(buffer, MESSAGE_SIZE, stdin);
        size_t newlineIndex = strcspn(buffer, "\n");
        buffer[newlineIndex] = '\0';

        // Verifica se o usuário deseja sair
        if (feof(stdin) || strcmp(buffer, "/quit") == 0) {
            printf("Desconectado\n");
            pthread_cancel(receive_thread);
            break;
        }

        // Enviar mensagem ao servidor
        while(1){
            if (send(client_socket, buffer, strlen(buffer) + 1, 0) < 0) {
                printf("Falha ao enviar mensagem.\n");
                break;
            }
            sleep(1);
            if(newlineIndex < (MESSAGE_SIZE - 1)) break;
            memset(buffer, 0, BUFFER_SIZE);
            fgets(buffer, MESSAGE_SIZE, stdin);
            newlineIndex = strcspn(buffer, "\n");
            if(!newlineIndex) break;
            buffer[newlineIndex] = '\0';
        }
    }

    // Aguardar a finalização da thread de recebimento de mensagens
    pthread_join(receive_thread, NULL);

    // Fechar socket
    close(client_socket);

    return 0;
}