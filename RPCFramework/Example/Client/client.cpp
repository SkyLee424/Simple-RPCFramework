#include "Test.hpp"

int main(int argc, char* argv[])
{
    if(argc != 4)
    {
        // e.g: ./clnt 192.168.124.114 1145 1
        std::cerr << "Usage: [ip][port][opinion]" << std::endl;
        return -1;
    }
    std::string ip(argv[1]);
    uint16_t port = std::stoi(argv[2]);
    int opinion = std::stoi(argv[3]);

    std::chrono::steady_clock::time_point start;
    try
    {
        switch (opinion)
        {
        case 0:
            start = std::chrono::steady_clock::now();
            testBasicFeature(ip, port);
            break;
        case 1:
        {
            int maxConnection;
            std::cout << "Input number of connections to test: ";
            std::cin >> maxConnection;
            start = std::chrono::steady_clock::now();
            testMaxConnections(ip, port, maxConnection);
            break;
        }
        case 2:
        {
            int threadNum, clntNum;
            std::cout << "Input number of thread to test: ";
            std::cin >> threadNum;
            std::cout << "Input number of clients in a single thread: ";
            std::cin >> clntNum;
            start = std::chrono::steady_clock::now();
            testConcurrency(ip, port, threadNum, clntNum);
            break;
        }
        case 3:
        {
            int threadNum, clntNum;
            std::cout << "Input number of thread to test: ";
            std::cin >> threadNum;
            std::cout << "Input number of clients in a single thread: ";
            std::cin >> clntNum;
            start = std::chrono::steady_clock::now();
            testConcurrency_1(ip, port, threadNum, clntNum);
            break;
        }
        default:
            start = std::chrono::steady_clock::now();
            std::cout << "No such opinion, available opinion: 0, 1, 2, 3" << std::endl;
            break;
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    auto end = std::chrono::steady_clock::now();
    std::cout << "Total cost time(ms): " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;
}