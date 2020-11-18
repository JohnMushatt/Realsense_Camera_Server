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
#include <sys/stat.h>
#include <fstream>

using namespace std::chrono;

std::string Camera_Server::Frame_Metadata::metadata_to_string() {
    return std::string(
            std::to_string(this->_local_frame_number) + "_" + std::to_string(this->_device_frame_number) + "_" +
            std::to_string(this->_timestamp));
}

Camera_Server::Camera_Server(std::string_view ip, std::size_t frame_buffer_size,
                             Camera_Server::Camera_Server_Operating_Mode mode) {
    this->mode = mode;
    this->frame_buffer.resize(frame_buffer_size);
    this->camera_server_instance.message_map.insert(
            std::make_pair(std::string("SEND_FRAME"), &Camera_Server::SEND_FRAME));
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
    /**
     * TODO Add exception throw and go into error thread to connection failure
     */
    if (!good_connection) {
        std::cout << "[-] Failed to connect to dashboard app" << std::endl;
        return EXIT_FAILURE;
    } else {
        std::cout << "[+] Connected to dashboard app, starting network thread" << std::endl;
    }

    std::thread network_thread(&Camera_Server::network_handler, this);
    std::cout << "[+] Starting camera thread" << std::endl;
    std::thread camera_thread(&Camera_Server::camera_handler, this);


    network_thread.join();
    camera_thread.join();
    //network_thread.join();
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


void Camera_Server::CONNECT(Server::Network_Message request) {

}

void Camera_Server::SEND_FRAME(Server::Network_Message request) {

}

int Camera_Server::network_handler() try {

    FILE *file_ptr;
    while (1) {
        /**
         * Attempt to acquire lock
         */
        std::cout << "[+] Network Handler thread (" << std::this_thread::get_id << ") is attempting to acquire lock"
                  << std::endl;
        std::unique_lock<std::mutex> lock(this->frame_buffer_lock);
        std::cout << "[+] Network Handler thread (" << std::this_thread::get_id << ") has acquired lock"
                  << std::endl;
        /**
         * When lock is acquired, wait for camera thread to finish filling up frame buffer ~50% then wake up
         */
        std::cout << "[+] Network Handler thread (" << std::this_thread::get_id
                  << ") is waiting on camera thread conditional"
                  << std::endl;
        this->frame_buffer_ready_cond.wait(lock);
        std::cout << "[+] Network Handler thread (" << std::this_thread::get_id << ") has woken up via .notify_one"
                  << std::endl;
        /*
        std::size_t size_error = frame_buffer.size();
        std::cout << "Current buffer size: " << size_error << std::endl;
        Frame_Metadata temp = frame_buffer.at(0);
        file_ptr = fopen(frame_buffer.at(0)._file_path.c_str(), "r");

        struct stat stat_buf;
        int64_t rc = stat(frame_buffer.at(0)._file_path.c_str(), &stat_buf);
        int64_t file_size = stat_buf.st_size;
        this->camera_server_instance.internal_buffer.resize(file_size);
        this->camera_server_instance.internal_buffer.clear();
        this->camera_server_instance.internal_buffer.;
         */

        std::ifstream file(frame_buffer.at(0)._file_path);
        std::ostringstream string_stream;
        string_stream << file.rdbuf();
        const std::string &s = string_stream.str();
        this->camera_server_instance.internal_buffer = std::vector<char>(s.begin(), s.end());
        std::string header_msg = std::to_string(this->camera_server_instance.internal_buffer.size());
        int64_t size_ack = send(this->camera_server_instance.server_file_descriptor, &header_msg[0], header_msg.size(),
                                0);
        int64_t bytes_sent = 0;
        while (bytes_sent < this->camera_server_instance.internal_buffer.size()) {
            bytes_sent += send(this->camera_server_instance.server_file_descriptor,
                               &this->camera_server_instance.internal_buffer.at(0),
                               this->camera_server_instance.internal_buffer.size(), 0);
            std::cout << "[+] Network thread sent " << bytes_sent << " bytes to dashboard" << std::endl;
        }
        std::cout << "[+] Network thread finished sending " << bytes_sent << " bytes to dashboard" << std::endl;

    }
}
catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

int Camera_Server::camera_handler() try {
    /**
     * Start Realsense 2 Camera pipeline
     * This will tell the api to start capturing frames from the camera
     */
    rs2::pipeline_profile pipe_profile = this->RS2_pipeline.start();
    while (1) {
        /**
         * Get buffer mutex and lock it
         */
        std::cout << "[+] Camera Handler thread (" << std::this_thread::get_id << ") is attempting to acquire lock"
                  << std::endl;
        std::unique_lock<std::mutex> lock(this->frame_buffer_lock);
        std::cout << "[+] Camera Handler thread (" << std::this_thread::get_id << ") has acquired lock"
                  << std::endl;
        for (size_t iteration = 0; iteration < 4; iteration++) {
            auto start = std::chrono::high_resolution_clock::now();
            this->camera_server_compute_pointcloud();
            auto end = std::chrono::high_resolution_clock::now();
            double elapsed_seconds = std::chrono::duration_cast<
                    std::chrono::duration<double> >(end - start).count();
            std::cout << "[+] Frame to .ply took " << elapsed_seconds << " seconds" << std::endl;

        }
        std::cout << "[+] Camera Handler thread (" << std::this_thread::get_id << ") is attempting to unlock lock"
                  << std::endl;
        lock.unlock();
        std::cout << "[+] Camera Handler thread (" << std::this_thread::get_id << ") has unlocked lock"
                  << std::endl;

        if (this->frame_buffer_index == 4) {
            this->frame_buffer_index = 0;
            std::cout << "[+] Frame buffer is full and camera thread is signaling network thread for work" << std::endl;
            this->frame_buffer_ready_cond.notify_one();
            this->frame_buffer_ready_cond.wait(lock);
        }

    }
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

    // Get frame buffer number
    this->frame_buffer.at(this->frame_buffer_index)._device_frame_number = frames.get_frame_number();
    // Get the local frame buffer number. Calculated as (_device_frame_number modulo frame_buffer.size())
    this->frame_buffer.at(this->frame_buffer_index)._local_frame_number =
            this->frame_buffer.at(this->frame_buffer_index)._device_frame_number % this->frame_buffer.size();
    this->frame_buffer.at(this->frame_buffer_index)._timestamp = std::time(nullptr);

    std::string file_header("framedata_");
    std::string file_extension(".ply");
    std::string frame_metadata_string = this->frame_buffer.at(this->frame_buffer_index).metadata_to_string();
    std::string file_name = file_header.append(frame_metadata_string).append(file_extension);
    std::string file_path = std::string("../framedata/").append(file_name);

    this->frame_buffer.at(this->frame_buffer_index)._file_path = file_path;

    std::cout << "Current file version: " << file_name << std::endl;
    this->RS2_points.export_to_ply(file_path, color);
    this->frame_buffer_index++;
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