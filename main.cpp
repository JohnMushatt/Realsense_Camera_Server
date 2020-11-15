#include <iostream>
#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>
#include <stdio.h>
#include <memory>
#include <camera_server.h>
#include <pthread.h>

int main() {
    std::string control_computer_ip = "130.215.249.199";
    std::unique_ptr<Camera_Server> cm_server = std::make_unique<Camera_Server>(control_computer_ip,Camera_Server::DEBUG);
    cm_server->camera_server_start();



    return 0;
}
