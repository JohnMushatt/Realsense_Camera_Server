//
// Created by johnm on 11/12/2020.
//

#include <unistd.h>
#include <string.h>
/**
 * C++ headers
 */
#include "camera_server.h"
#include <cerrno>
#include <iostream>
#include <string_view>
#include <ctime>
#include <cctype>
#include <algorithm>
#include <thread>

/**
 *
 */
struct Frame_Metadata {
    uint64_t _device_frame_number; /*!< Real frame number generated from Intel Librealsense API */
    uint64_t _local_frame_number; /*!< 64-bit unsigned integer [0-1023] that refers to an "index" of a 1024 length "buffer" */
    uint64_t _timestamp; /*!< 64-bit unsigned integer timestamp */
    std::string metadata_to_string();
};

std::string Frame_Metadata::metadata_to_string() {
    return std::string(
            std::to_string(this->_local_frame_number) + "_" + std::to_string(this->_device_frame_number) + "_" +
            std::to_string(this->_timestamp));
}

Camera_Server::Camera_Server(std::string_view ip, Camera_Server::Camera_Server_Operating_Mode mode) {
    this->mode = mode;

}

bool Camera_Server::camera_server_connect() {

    this->camera_server_instance.server_file_descriptor = 0;
    this->camera_server_instance.status = 0;

    this->camera_server_instance.internal_buffer = {0};
    if ((this->camera_server_instance.server_file_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return EXIT_FAILURE;
    }

    memset(&this->camera_server_instance.server_addr, '0', sizeof(this->camera_server_instance.server_addr));

    this->camera_server_instance.server_addr.sin_family = AF_INET;
    this->camera_server_instance.server_addr.sin_port = htons(8080);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "130.215.249.199", &this->camera_server_instance.server_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return EXIT_FAILURE;
    }
    if (connect(this->camera_server_instance.server_file_descriptor,
                (struct sockaddr *) &this->camera_server_instance.server_addr,
                sizeof(this->camera_server_instance.server_addr)) < 0) {
        printf("\nConnection Failed\n");
        return EXIT_FAILURE;
    }
    printf("\nConnection Succeeded\n");
    return true;
}

Camera_Server::~Camera_Server() {

}

bool Camera_Server::camera_server_start() try {
     bool good_connection = this->camera_server_connect();
    if (!good_connection) {
        return EXIT_FAILURE;
    }
    std::thread network_thread(&Camera_Server::network_handler, this);
    network_thread.join();
    bool camera_connected = this->RS2_pipeline.start();
    this->frame_buffer.resize(1024);
    if (this->frame_buffer.size() != 1024) {
        printf("[-] Failed to allocate frame buffer\n");
        return EXIT_FAILURE;
    }
    if (!camera_connected) {
        return EXIT_FAILURE;
    }
    std::thread camera_thread(&Camera_Server::camera_handler, this);

    return EXIT_SUCCESS;
} catch (const rs2::error &e) {
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    "
              << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

void Camera_Server::camera_handler() {

}

void Camera_Server::network_handler() {

}


bool Camera_Server::execute_frame_to_ply() {
    return camera_server_compute_pointcloud();
}

bool Camera_Server::camera_server_compute_pointcloud() try {
    // Wait for the next set of frames from the camera
    rs2::frameset frames = this->RS2_pipeline.wait_for_frames();
    // Get color data for current frame
    rs2::video_frame color = frames.get_color_frame();

    // For cameras that don't have RGB sensor, we'll map the pointcloud to infrared instead of color
    if (!color)
        color = frames.get_infrared_frame();

    // Tell pointcloud object to map to this color frame
    this->RS2_pointcloud.map_to(color);

    rs2::depth_frame depth_frame = frames.get_depth_frame();

    // Generate the pointcloud and texture mappings
    this->RS2_points = this->RS2_pointcloud.calculate(depth_frame);

    Frame_Metadata metadata;

    metadata._device_frame_number = frames.get_frame_number();
    metadata._local_frame_number = metadata._device_frame_number % 1024;
    metadata._timestamp = std::time(nullptr);

    std::string file_header("framedata_");
    std::string file_extension(".ply");
    std::string frame_metadata_string = metadata.metadata_to_string();
    std::string file_name = file_header.append(frame_metadata_string).append(file_extension);
    std::string file_path = std::string("../framedata/").append(file_name);
    std::cout << "Current file version: " << file_name << std::endl;
    this->RS2_points.export_to_ply(file_path, color);

    return EXIT_SUCCESS;
}
catch (const rs2::error &e) {
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    "
              << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}