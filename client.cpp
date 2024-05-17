#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <stdarg.h>

#define BUF_SIZE 1024
#define SERVER_PORT 5208 // Порт сервера
#define IP "127.0.0.1"

void send_msg(SOCKET sock);
void recv_msg(SOCKET sock);
int output(const char* arg, ...);
int error_output(const char* arg, ...);
void error_handling(const std::string& message);

std::string name = "DEFAULT";
std::string msg;

int main(int argc, const char** argv) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        error_handling("WSAStartup() failed!");
        return 1;
    }

    SOCKET sock;
    struct sockaddr_in serv_addr;
    // argc -  количество аргументов, переданных программе через командную строку при ее запуске
    // argv[0] будет содержать "./client.exe", а argv[1] будет содержать "client_name" - всего 2 элемента
    if (argc != 2) {
        error_output("Usage : %s <Name> \n", argv[0]);
        return 1;
    }

    // Устанавливаем имя клиента
    name = "[" + std::string(argv[1]) + "]";

    // Создаем сокет для клиента для TCP-соединения
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        error_handling("socket() failed!");
        return 1;
    }

    // Заполняем структуру адреса сервера
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, IP, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(SERVER_PORT);

    // Подключаемся к серверу
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        error_handling("connect() failed!");
        return 1;
    }

    // Отправляем серверу имя клиента
    std::string my_name = "#new client:" + std::string(argv[1]);
    send(sock, my_name.c_str(), my_name.length() + 1, 0);

    // Создаем потоки для отправки и приема сообщений
    std::thread snd(send_msg, sock);
    std::thread rcv(recv_msg, sock);

    snd.join();
    rcv.join();

    // Закрываем сокет
    closesocket(sock);

    WSACleanup();
    return 0;
}

void send_msg(SOCKET sock) {
    while (1) {
        std::getline(std::cin, msg);
        if (msg == "Quit" || msg == "quit") {
            closesocket(sock);
            return;
        }
        // Формируем сообщение в формате [name] message
        std::string name_msg = name + " " + msg;
        send(sock, name_msg.c_str(), name_msg.length() + 1, 0);
    }
}

void recv_msg(SOCKET sock) {
    char name_msg[BUF_SIZE];
    while (1) {
        int str_len = recv(sock, name_msg, BUF_SIZE, 0);
        if (str_len == SOCKET_ERROR || str_len == 0) {
            // Ошибка при получении данных или соединение закрыто
            return;
        }
        // Добавляем символ завершения строки
        name_msg[str_len] = '\0';
        std::cout << std::string(name_msg) << std::endl;
    }
}

int output(const char* arg, ...) {
    int res;
    va_list ap;
    va_start(ap, arg);
    res = vfprintf(stdout, arg, ap);
    va_end(ap);
    return res;
}

int error_output(const char* arg, ...) {
    int res;
    va_list ap;
    va_start(ap, arg);
    res = vfprintf(stderr, arg, ap);
    va_end(ap);
    return res;
}

void error_handling(const std::string& message) {
    std::cerr << message << std::endl;
}
