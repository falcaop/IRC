#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

int main() {
    int client_socket;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE];

    // Cria o socket do cliente
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    // Configura o endereço do servidor
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");  // Altere o endereço IP para o do servidor
    server_address.sin_port = htons(6667);  // Altere a porta para a mesma porta do servidor

    // Conecta ao servidor
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Erro ao conectar ao servidor");
        exit(EXIT_FAILURE);
    }

    printf("Conectado ao servidor IRC. Digite 'sair' para encerrar.\n");

    // Configura o conjunto de descritores de arquivo (fd_set)
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(client_socket, &read_fds);

    int max_fd = (client_socket > STDIN_FILENO) ? client_socket : STDIN_FILENO;

    while (1) {
        // Espera por atividade em algum socket
        fd_set temp_fds = read_fds;
        int activity = select(max_fd + 1, &temp_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Erro ao executar select");
            exit(EXIT_FAILURE);
        }

        // Verifica se há entrada do usuário
        if (FD_ISSET(STDIN_FILENO, &temp_fds)) {
            memset(buffer, 0, sizeof(buffer));

            // Lê a entrada do usuário
            fgets(buffer, sizeof(buffer), stdin);

            // Remove o caractere de nova linha da entrada
            buffer[strcspn(buffer, "\n")] = '\0';

            // Verifica se o usuário deseja sair
            if (strcmp(buffer, "/quit") == 0) {
                break;
            }

            // Envia a mensagem para o servidor
            if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
                perror("Erro ao enviar mensagem para o servidor");
                exit(EXIT_FAILURE);
            }

            
        }

        // Verifica se há mensagem recebida do servidor
        if (FD_ISSET(client_socket, &temp_fds)) {
            memset(buffer, 0, sizeof(buffer));

            // Recebe a mensagem do servidor
            int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0) {
                // O servidor fechou a conexão
                break;
            }

            // Exibe a mensagem recebida do servidor
            printf("Mensagem do servidor: %s\n", buffer);
        }
    }

    // Fecha o socket do cliente
    close(client_socket);

    return 0;
}
