#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define HEADER_SIZE 52
#define MESSAGE_SIZE 4096
#define BUFFER_SIZE HEADER_SIZE+MESSAGE_SIZE

int client_socket;
pthread_t receive_thread;

// função executada pela thread de recebimento de mensagens do servidor
void *receive_messages(void *arg);

// função principal
int main() {
    signal(SIGINT, SIG_IGN);

    struct sockaddr_in server_address;
    char server_ip[] = "127.0.0.1";
    int server_port = 12345;
    char buffer[BUFFER_SIZE];

    // criar socket do cliente
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        printf("Erro ao criar o socket do cliente.\n");
        return 1;
    }

    // configurar endereço do servidor
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(server_ip);
    server_address.sin_port = htons(server_port);

    // o usuário deve digitar /connect para estabelecer a conexão com servidor
    char *command;
    printf("Digite '/connect' para se conectar ao servidor ou '/exit' para sair.\n");
    while(1){
        printf("> ");
        scanf("%ms", &command);
        getchar();
        if(!strcmp(command, "/connect")){
            free(command);
            break;
        }      
        else if (!strcmp(command, "/exit")){
            free(command);
            exit(0);
        }
        free(command);
        printf("Digite '/connect' para se conectar ao servidor ou '/exit' para sair.\n");
    }

    // conexão com o servidor
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printf("Erro ao conectar-se ao servidor.\n");
        return 1;
    }
    printf(
        "Conectado ao servidor IRC. Digite '/nickname (apelido)' para definir o seu apelido, '/join (canal)' para se"
        " juntar a um canal ou criar um novo, ou '/quit' para encerrar.\n"
    );

    // iniciar a thread de recebimento de mensagens
    if (pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        printf("Erro ao criar a thread de recebimento de mensagens.\n");
        return 1;
    }

    // leitura de mensagens do usuário
    while (1) {
        // limpar o buffer e ler mensagem
        memset(buffer, 0, BUFFER_SIZE);
        fgets(buffer, MESSAGE_SIZE, stdin);
        size_t newlineIndex = strcspn(buffer, "\n");
        buffer[newlineIndex] = '\0';

        // verificar se usuário deseja se desconectar
        if (feof(stdin) || strcmp(buffer, "/quit") == 0) {
            printf("Desconectado\n");
            pthread_cancel(receive_thread);
            break;
        }

        // enviar mensagem ao servidor
        // se o tamanho da mensagem for maior que o tamanho permitido, ela é dividida e
        // cada parte é enviada indiviudalmente ao servidor
        while(1){
            if (send(client_socket, buffer, strlen(buffer) + 1, 0) < 0) {
                printf("Erro ao enviar mensagem.\n");
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

    // fechar socket
    close(client_socket);

    return 0;
}

void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        // recebimento da mensagem
        memset(buffer, 0, BUFFER_SIZE);
        int read_size = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (read_size <= 0){
            break;
        }

        // exibição da mensagem na tela
        printf("%s\n", buffer);
        fflush(stdout);
    }

    close(client_socket);
    printf("Desconectado do servidor.\n");
    fflush(stdout);
    exit(0);
}