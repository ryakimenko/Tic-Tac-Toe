#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

#include "net.h"

namespace {

struct Board {
    char values[3][3];

    Board()
    {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                values[i][j] = '_';
            }
        }
    }
} static board;

bool isFirst;

void printBoard()
{
    std::cout << "Board:\n";
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            std::cout << board.values[i][j] << ' ';
        }
        std::cout << "\n";
    }
}

int parseServer(char* buff)
{
    auto find = strstr(buff, "\r\n");
    std::string head(buff, find - buff);

    if (head == "OK") {
        if (find[2] == '1') {
            return 1;
        }
        return 0;
    }
    return -1;
}

std::optional<struct sockaddr_in> parseReserve(char* buff)
{
    auto find = strstr(buff, "\r\n");
    std::string head(buff, find - buff);

    if (head != "INIT") {
        return std::nullopt;
    }
    find += 2;
    auto portFind = strstr(find, "\r\n");
    std::string p(find, portFind - find);

    int port = stoi(p);

    if (port < 0 || port > 65535)
        return std::nullopt;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    printf("Reserve server port: %d\n", port);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    portFind += 2;
    auto orderFind = strstr(portFind, "\r\n");
    if (orderFind[-1] == '1') {
        isFirst = true;
    } else {
        isFirst = false;
    }

    return addr;
}

void swapToReserve(struct sockaddr_in& addr, char* old, int& s)
{
    close(s);
    s = socket(AF_INET, SOCK_STREAM, 0);
    int flag = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    connect(s, (struct sockaddr*)&addr, sizeof(addr));
    printf("Connect new\n");
    perror("");
    auto dReserve = std::string("RESERVE_FROM\r\n") + old + "\r\n" +
                    (isFirst ? "1" : "2") + "\r\n\r\n";
    printf("Send data\n");
    send(s, dReserve.c_str(), dReserve.size(), 0);
    perror("");
}

std::optional<std::pair<int, int>> parseStep(char const* buff)
{
    auto find = strstr(buff, "\r\n");
    std::string head(buff, find - buff);

    if (head == "STEP") {
        find += 2;
        auto value = strstr(find, "\r\n");

        if (value - find < 3)
            return std::nullopt;
        int row = find[0] - '0';
        int col = find[2] - '0';

        if (row < 0 || row > 2 || col < 0 || col > 2)
            return std::nullopt;

        return std::make_pair(row, col);
    } else if (head == "OK") {
        return std::make_pair(-1, -1);
    }
    return std::nullopt;
}

} // namespace

int start(int argc, char** argv)
{
    int port = atoi(argv[1]);
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    char buffer[1024] = {};

    int flag = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    auto val = connect(s, (struct sockaddr*)&addr, sizeof(addr));

    printf("Connect to server %d\n", val);
    perror("");

    recv(s, buffer, std::size(buffer), 0);
    printf("Recover from server\n");
    auto reserveAddr = parseReserve(buffer);

    printf("Get reserve\n");

    while (true) {
        if (!isFirst) {
            auto ret = recv(s, buffer, std::size(buffer), 0);
            if (ret <= 0) {
                if (reserveAddr) {
                    printf("Swap to reserve\n");
                    swapToReserve(*reserveAddr, argv[1], s);
                    continue;
                }
            }
            auto val = parseStep(buffer);
            if (val) {
                if (val->first == -1 && val->second == -1) {
                    return 0;
                }
                board.values[val->first][val->second] = isFirst ? 'o' : 'x';
            }
            printBoard();
        }

        int row = -1, col = -1;

        while (true) {
            scanf("%d %d", &row, &col);
            if (row < 0 || row > 2 || col < 0 || col > 2)
                continue;
            if (board.values[row][col] != '_')
                continue;
            break;
        }

        printf("Input\n");
        auto d = "STEP\r\n" + std::to_string(row) + " " +
                 std::to_string(col) + "\r\n\r\n";
        auto ret = send(s, d.c_str(), d.size(), 0);

        printf("Send server ret: %ld\n", ret);

        if (ret <= 0) {
            if (reserveAddr) {
                printf("Swap to reserve\n");
                swapToReserve(*reserveAddr, argv[1], s);
                send(s, d.c_str(), d.size(), 0);
            }
        }
        ret = recv(s, buffer, std::size(buffer), 0);
        printf("Recover server: %ld\n", ret);
        if (ret <= 0) {
            printf("Swap to reserve\n");
            swapToReserve(*reserveAddr, argv[1], s);
            printf("Send data\n");
            std::this_thread::sleep_for(std::chrono::microseconds(2000));
            send(s, d.c_str(), d.size(), 0);
            printf("Recover answer from reserve\n");
            recv(s, buffer, std::size(buffer), 0);
            printf("End send data\n");
        }
        ret = parseServer(buffer);
        printf("Server data: %s\n", buffer);
        printf("Parse server: %ld\n", ret);
        if (ret == 0) {
            board.values[row][col] = isFirst ? 'x' : 'o';
        } else if (ret == 1) {
            return 0;
        }
        printBoard();

        if (isFirst) {
            printf("Recover other step\n");
            ret = recv(s, buffer, std::size(buffer), 0);
            if (ret <= 0) {
                if (reserveAddr) {
                    printf("Swap reserve\n");
                    swapToReserve(*reserveAddr, argv[1], s);
                    recv(s, buffer, std::size(buffer), 0);
                }
            }
            auto val = parseStep(buffer);
            if (val) {
                if (val->first == -1 && val->second == -1)
                    return 0;
                board.values[val->first][val->second] = isFirst ? 'o' : 'x';
            }
            printBoard();
        }
    }
}
