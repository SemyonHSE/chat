#include <iostream>
#include <cstring>
#include <thread>
#include <mutex>
#include <WinSock2.h>
#include <unordered_map>
#include <string>
#include <fstream>

#define SERVER_PORT 5208 // Порт сервера
#define BUF_SIZE 1024    // Буфер будет иметь размер 1024 байта
#define MAX_CLNT 256     // Максимальное количество подключений

void handle_clnt(SOCKET clnt_sock);
void send_msg(const std::string& msg, const std::string& sender = "");
void save_msg_to_history(const std::string& msg, const std::string& client_name);
std::string get_chat_history(const std::string& client_name);
int output(const char* arg, ...);
int error_output(const char* arg, ...);
void error_handling(const std::string& message);

int clnt_cnt = 0;
std::mutex mtx;
std::unordered_map<std::string, SOCKET> clnt_socks;

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        error_handling("WSAStartup() failed!");
        return 1;
    }

    SOCKET serv_sock;
    serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serv_sock == INVALID_SOCKET) {
        error_handling("socket() failed!");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in serv_addr, clnt_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERVER_PORT);

    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        error_handling("bind() failed!");
        closesocket(serv_sock);
        WSACleanup();
        return 1;
    }

    printf("The server is running on port %d\n", SERVER_PORT);

    if (listen(serv_sock, MAX_CLNT) == SOCKET_ERROR) {
        error_handling("listen() error!");
        closesocket(serv_sock);
        WSACleanup();
        return 1;
    }

    SOCKET clnt_sock;
    int clnt_addr_size;
    while (1) {
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if (clnt_sock == INVALID_SOCKET) {
            error_handling("accept() failed!");
            continue;
        }

        std::thread th(handle_clnt, clnt_sock);
        th.detach();

        output("Connected client IP: %s\n", inet_ntoa(clnt_addr.sin_addr));
    }

    closesocket(serv_sock);
    WSACleanup();
    return 0;
}

void handle_clnt(SOCKET clnt_sock) {
    char msg[BUF_SIZE];
    bool quit_flag = false;
    std::string client_name;

    while (!quit_flag && recv(clnt_sock, msg, sizeof(msg), 0) > 0) {
        std::string message(msg);
        if (message.find("#new client:") == 0) {
            client_name = message.substr(12);
            if (clnt_socks.find(client_name) == clnt_socks.end()) {
                output("The name of socket %d: %s\n", clnt_sock, client_name.c_str());
                mtx.lock();
                clnt_socks[client_name] = clnt_sock;

                // Check if history file exists, if not create it
                std::ofstream file(client_name + "_history.txt", std::ios::app);
                if (!file.is_open()) {
                    error_output("Failed to create history file.\n");
                }
                file.close();

                // Send chat history to the new client
                std::string history = get_chat_history(client_name);
                send(clnt_sock, history.c_str(), history.length() + 1, 0);

                // Send general chat history to the new client
                std::string general_history = get_chat_history("general");
                send(clnt_sock, general_history.c_str(), general_history.length() + 1, 0);

                mtx.unlock();
            } else {
                std::string error_msg = client_name + " exists already. Please quit and enter with another name!";
                send(clnt_sock, error_msg.c_str(), error_msg.length() + 1, 0);
                closesocket(clnt_sock);
                return;
            }
        } else if (message == "quit" || message == "Quit") {
            quit_flag = true;
        } else {
            send_msg(message, client_name);
            save_msg_to_history(message, client_name);
        }
    }

    if (quit_flag) {
        std::string leave_msg = "Client " + client_name + " leaves the chat room";
        send_msg(leave_msg, client_name);
        save_msg_to_history(leave_msg, client_name);
        output("Client %s leaves the chat room\n", client_name.c_str());

        mtx.lock();
        clnt_socks.erase(client_name);
        mtx.unlock();

        closesocket(clnt_sock);
    }
}


void send_msg(const std::string& msg, const std::string& sender) {
    mtx.lock();
    std::string pre = "@";
    int first_space = msg.find_first_of(" ");
    if (first_space != std::string::npos && msg.compare(first_space + 1, 1, pre) == 0) {
        int space = msg.find_first_of(" ", first_space + 1);
        if (space != std::string::npos) {
            std::string receive_name = msg.substr(first_space + 2, space - first_space - 2);
            std::string send_name = msg.substr(0, first_space);
            if (clnt_socks.find(receive_name) == clnt_socks.end()) {
                std::string error_msg = "[error] there is no client named " + receive_name;
                send(clnt_socks[send_name], error_msg.c_str(), error_msg.length() + 1, 0);
            }
            else {
                // Send message to the recipient
                send(clnt_socks[receive_name], msg.c_str(), msg.length() + 1, 0);

                // Save message to sender's history
                save_msg_to_history(msg, send_name);
            }
        }
    } else {
        // Send message to all clients (excluding sender)
        for (auto& client : clnt_socks) {
            if (client.first != sender) {
                send(client.second, msg.c_str(), msg.length() + 1, 0);
            }
        }

        // Save message to general history
        save_msg_to_history(msg, "general");
    }
    mtx.unlock();
}


void save_msg_to_history(const std::string& msg, const std::string& client_name) {
    std::string file_name = client_name + "_history.txt";
    std::ofstream file(file_name, std::ios::app);
    if (file.is_open()) {
        file << msg << std::endl;
        file.close();
    } else {
        error_output("Failed to open history file for writing.\n");
    }
}

std::string get_chat_history(const std::string& client_name) {
    std::string file_name = client_name + "_history.txt";
    std::ifstream file(file_name);
    std::string history, line;
    if (file.is_open()) {
        while (getline(file, line)) {
            history += line + "\n";
        }
        file.close();
    } else {
        error_output("Failed to open history file for reading.\n");
    }
    return history;
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
