#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>

#define MAX_CLIENTS 10
#define HEADER_SIZE 52
#define MESSAGE_SIZE 16

int main() {
    int server_socket, client_sockets[MAX_CLIENTS], client_count = 0;
    struct sockaddr_in server_address, client_address;
    char buffer[HEADER_SIZE + MESSAGE_SIZE];

    // Cria o socket do servidor
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Erro ao criar socket");
        exit(EXIT_FAILURE);
    }

    // Configura o endereço do servidor
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(6667);

    // Associa o socket do servidor ao endereço
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Erro ao associar socket");
        exit(EXIT_FAILURE);
    }

    // Escuta por conexões
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Erro ao escutar por conexões");
        exit(EXIT_FAILURE);
    }

    printf("Servidor iniciado. Aguardando conexões...\n");

    fd_set read_fds;
    int max_fd, activity;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        max_fd = server_socket;

        // Adiciona os sockets de clientes ao conjunto
        for (int i = 0; i < client_count; i++) {
            int client_socket = client_sockets[i];
            if (client_socket > 0) {
                FD_SET(client_socket, &read_fds);
                if (client_socket > max_fd) {
                    max_fd = client_socket;
                }
            }
        }

        // Espera por atividade em algum dos sockets
        activity = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Erro ao executar select");
            exit(EXIT_FAILURE);
        }

        // Verifica se há novas conexões
        if (FD_ISSET(server_socket, &read_fds)) {
            socklen_t client_address_len = sizeof(client_address);

            // Aceita uma conexão de cliente
            int client_socket = accept(server_socket, (struct sockaddr*)&client_address, &client_address_len);
            if (client_socket < 0) {
                perror("Erro ao aceitar conexão");
                exit(EXIT_FAILURE);
            }

            // Adiciona o socket do cliente ao array de sockets
            client_sockets[client_count] = client_socket;
            client_count++;

            printf("Novo cliente conectado. Socket: %d, IP: %s, Porta: %d\n",
                   client_socket,
                   inet_ntoa(client_address.sin_addr),
                   ntohs(client_address.sin_port));
        }

        // Verifica se há atividade em algum dos clientes
        for (int i = 0; i < client_count; i++) {
            int client_socket = client_sockets[i];

            if (FD_ISSET(client_socket, &read_fds)) {
                memset(buffer, 0, sizeof(buffer));

                // Recebe a mensagem do cliente
                int bytes_received = recv(client_socket, buffer, MESSAGE_SIZE, 0);
                if (bytes_received <= 0) {
                    // Erro ou conexão fechada pelo cliente
                    printf("Cliente desconectado. Socket: %d\n", client_socket);
                    close(client_socket);

                    // Remove o socket do cliente do array de sockets
                    for (int j = i; j < client_count - 1; j++) {
                        client_sockets[j] = client_sockets[j + 1];
                    }

                    client_count--;
                } else {
                    if (strcmp(buffer, "/ping") == 0){
                        fd_set write_set;
                        FD_ZERO(&write_set);
                        FD_SET(client_socket, &write_set);
                        int select_result = select(client_socket + 1, NULL, &write_set, NULL, NULL);
                        if (select_result == -1) {
                            perror("Erro ao enviar mensagem para o servidor");
                            exit(EXIT_FAILURE);
                        }
                        send(client_socket, "pong", 6, 0);
                        continue;
                    }
                    // Envia a mensagem recebida para todos os clientes conectados
                    for (int j = 0; j < client_count; j++) {
                        int target_socket = client_sockets[j];

                        // dps
                        char nickname[10];
                        sprintf(nickname,"%d", client_socket);
                        
                        char message[HEADER_SIZE + MESSAGE_SIZE];
                        strcpy(message, nickname);
                        strcat(message, ": ");
                        strcat(message, buffer);
                        int size = bytes_received + 2 + strlen(nickname);
                        printf("%d - %s\n", size, message);
                        fd_set write_set;
                        FD_ZERO(&write_set);
                        FD_SET(target_socket, &write_set);
                        int select_result = select(target_socket + 1, NULL, &write_set, NULL, NULL);
                        if (select_result == -1) {
                            perror("Erro ao enviar mensagem para o servidor");
                            exit(EXIT_FAILURE);
                        }
                        send(target_socket, message, size, 0);
                    }
                }
            }
        }
    }

    // Fecha todos os sockets de clientes restantes
    for (int i = 0; i < client_count; i++) {
        close(client_sockets[i]);
    }

    // Fecha o socket do servidor
    close(server_socket);

    return 0;
}
