#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
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

int firstPlayer = -1;
int secondPlayer = -1;
bool firstStep = true;

std::mutex reserveMutex;

struct StepResult {
    int row;
    int col;
    bool noAnswer;

    StepResult(int row, int col, bool noAnswer = false)
        : row(row), col(col), noAnswer(noAnswer)
    { }
};

bool checkForWin(int playerSock)
{
    if (playerSock == firstPlayer) {
        if (board.values[0][0] == 'x' && board.values[0][1] == 'x' &&
            board.values[0][2] == 'x')
            return true;
        if (board.values[1][0] == 'x' && board.values[1][1] == 'x' &&
            board.values[1][2] == 'x')
            return true;
        if (board.values[2][0] == 'x' && board.values[2][1] == 'x' &&
            board.values[2][2] == 'x')
            return true;
        if (board.values[0][0] == 'x' && board.values[0][1] == 'x' &&
            board.values[0][2] == 'x')
            return true;
        if (board.values[1][0] == 'x' && board.values[1][1] == 'x' &&
            board.values[1][2] == 'x')
            return true;
        if (board.values[2][0] == 'x' && board.values[2][1] == 'x' &&
            board.values[2][2] == 'x')
            return true;
        if (board.values[0][2] == 'x' && board.values[1][1] == 'x' &&
            board.values[2][0] == 'x')
            return true;
        if (board.values[0][0] == 'x' && board.values[1][1] == 'x' &&
            board.values[2][2] == 'x')
            return true;
        return false;
    } else {
        if (board.values[0][0] == 'o' && board.values[0][1] == 'o' &&
            board.values[0][2] == 'o')
            return true;
        if (board.values[1][0] == 'o' && board.values[1][1] == 'o' &&
            board.values[1][2] == 'o')
            return true;
        if (board.values[2][0] == 'o' && board.values[2][1] == 'o' &&
            board.values[2][2] == 'o')
            return true;
        if (board.values[0][0] == 'o' && board.values[0][1] == 'o' &&
            board.values[0][2] == 'o')
            return true;
        if (board.values[1][0] == 'o' && board.values[1][1] == 'o' &&
            board.values[1][2] == 'o')
            return true;
        if (board.values[2][0] == 'o' && board.values[2][1] == 'o' &&
            board.values[2][2] == 'o')
            return true;
        if (board.values[0][2] == 'o' && board.values[1][1] == 'o' &&
            board.values[2][0] == 'o')
            return true;
        if (board.values[0][0] == 'o' && board.values[1][1] == 'o' &&
            board.values[2][2] == 'o')
            return true;
        return false;
    }
}

bool isMakeReserve(char const* buff)
{
    auto find = strstr(buff, "\r\n");
    std::string head(buff, find - buff);

    return head == "MAKE_RESERVE";
}

int isReserve(char const* buff)
{
    auto find = strstr(buff, "\r\n");
    std::string head(buff, find - buff);

    if (head == "RESERVE_FROM") {
        find += 2;
        find = strstr(find, "\r\n");
        find += 2;
        auto order = find[0] - '0';
        return order;
    }

    return 0;
}

std::optional<StepResult> parseClient(char const* buff)
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

        value += 2;
        find = strstr(value, "\r\n");
        bool noAnswer = false;
        if (find - value > 0) {
            std::string param(value, find - value);
            if (param == "no-answer") {
                printf("Find no-answer\n");
                noAnswer = true;
            } else
                return std::nullopt;
        }

        printf("Return noAnswer: %d\n", noAnswer);

        return StepResult(row, col, noAnswer);
    }
    return std::nullopt;
}

int makeReserve(int port, int serverPort)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    char buffer[1024] = {};

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    connect(s, (struct sockaddr*)&addr, sizeof(addr));

    auto msg =
        "MAKE_RESERVE\r\nport: " + std::to_string(serverPort) + "\r\n\r\n";
    send(s, msg.c_str(), msg.size(), 0);

    recv(s, buffer, std::size(buffer), 0);
    if (memcmp(buffer, "OK\r\n\r\n", strlen(buffer)) != 0) {
        return -1;
    }

    return s;
}

void clientCycle(int s, int reserveSock)
{
    char buff[1024] = {};

    while (true) {
        int ret = recv(s, buff, std::size(buff), 0);

        printf("Client recover: %d\n", ret);

        if (ret <= 0)
            return;

        auto step = parseClient(buff);
        printf("Step value: %d\n", step.has_value());
        if (!step) {
            if (isMakeReserve(buff)) {
                send(s, "OK\r\n\r\n", 6, 0);
                printf("Make reserve\n");
            } else if (int val = isReserve(buff)) {
                printf("Buffer: %s\n", buff);
                printf("Swap to reserve: %d\n", val);
                if (val == 1) {
                    firstPlayer = s;
                } else if (val == 2) {
                    secondPlayer = s;
                }
            } else {
                printf("Send failed\n");
                send(s, "FAILED\r\n\r\n", 10, 0);
            }
        } else {
            printf("Reserve sock: %d\n", reserveSock);
            if (s == firstPlayer && firstStep &&
                board.values[step->row][step->col] == '_') {
                board.values[step->row][step->col] = 'x';
            } else if (
                s == secondPlayer && !firstStep &&
                board.values[step->row][step->col] == '_') {
                board.values[step->row][step->col] = 'o';
            } else {
                send(s, "FAILED\r\n\r\n", 10, 0);
                continue;
            }
            if (checkForWin(s))
                if (s == firstPlayer) {
                    send(s, "Win\r\n\r\n", 8, 0);
                    send(secondPlayer, "Lose\r\n\r\n", 9, 0);
                } else {
                    send(s, "Win\r\n\r\n", 8, 0);
                    send(firstPlayer, "Lose\r\n\r\n", 9, 0);
                }
            if (reserveSock > 0) {
                printf("Send to reserve\n");
                auto& val = step.value();
                auto data = "STEP\r\n" + std::to_string(val.row) + " " +
                            std::to_string(val.col) + "\r\nno-answer\r\n\r\n";
                send(reserveSock, data.c_str(), data.size(), 0);
                printf("End send reserve\n");
            }
            printf("No answer: %d\n", step->noAnswer);
            if (!step->noAnswer) {
                printf("Send OK\r\n");
                auto other = s == secondPlayer ? firstPlayer : secondPlayer;
                printf("Other: %d\n", other);
                ret = send(other, buff, ret, 0);
                printf("Other ret: %d\n", (int)ret);
                if (checkForWin(s)) {
                    if (s == firstPlayer) {
                        send(s, "OK\r\n1\r\n", 7, 0);
                        send(secondPlayer, "OK\r\n2\r\n", 7, 0);
                    } else {
                        send(firstPlayer, "OK\r\n2\r\n", 7, 0);
                        send(secondPlayer, "OK\r\n1\r\n", 7, 0);
                    }
                } else {
                    send(s, "OK\r\n\r\n", 6, 0);
                }
            }

            firstStep = !firstStep;
        }
    }
}

} // namespace

int start(int argc, char** argv)
{
    int serverPort = atoi(argv[1]);
    char* reservePort = nullptr;
    int reserveSock = -1;
    if (argc > 2) {
        reservePort = argv[2];
        reserveSock = makeReserve(atoi(reservePort), serverPort);
        printf("Reserve sock: %d\n", reserveSock);
    }

    printf("Start server port: %d\n", serverPort);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    char buffer[1024] = {};

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(serverPort);

    auto bindResult = -1;

    while (bindResult < 0)
        bindResult = bind(s, (struct sockaddr*)&addr, sizeof(addr));

    printf("Bind result: %d\n", bindResult);
    perror("");

    listen(s, 10);

    while (true) {
        auto clientSock = accept(s, (struct sockaddr*)nullptr, nullptr);

        printf("Accepted\n");

        if (reservePort) {
            auto data = std::string("INIT\r\n") + reservePort + "\r\n" +
                        (firstPlayer > 0 ? "2" : "1") + "\r\n\r\n";
            send(clientSock, data.c_str(), data.size(), 0);

            if (firstPlayer > 0) {
                secondPlayer = clientSock;
            } else {
                firstPlayer = clientSock;
            }
        }

        std::thread t(clientCycle, clientSock, reserveSock);
        t.detach();
    }

    return 0;
}
