#include <iostream>
#include <memory>
#include <camera_server.h>

int main() {
    std::string control_computer_ip = "130.215.249.199";
    std::unique_ptr<Camera_Server> cm_server = std::make_unique<Camera_Server>(control_computer_ip, 4,
                                                                               Camera_Server::DEBUG);
    cm_server->camera_server_start();
    return 0;
}
