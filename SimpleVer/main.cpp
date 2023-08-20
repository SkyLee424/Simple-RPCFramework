#include "RPCServer.hpp"
#include "Procedure.hpp"

int main(void)
{
    RPCServer server("192.168.124.114", 1145);
    server.registerProcedure("add", add);
    server.registerProcedure("subtract", subtract);
    server.registerProcedure("multiply", multiply);
    server.registerProcedure("divide", divide);

    server.start();
}