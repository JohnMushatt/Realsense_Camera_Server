//
// Created by johnm on 11/12/2020.
//

#ifndef REALSENSE_SERVER_CAMERA_SERVER_H
#define REALSENSE_SERVER_CAMERA_SERVER_H

#include <librealsense2/rs.hpp>
#include <arpa/inet.h>
#include <string>
#include <string_view>
#include <mutex>
#include <atomic>
#include <map>
#include <condition_variable>
class Camera_Server {
public:
    enum Camera_Server_Operating_Mode {
        RELEASE, DEBUG
    };

    Camera_Server(std::string_view ip, std::size_t frame_buffer_size, Camera_Server::Camera_Server_Operating_Mode mode);

    ~Camera_Server();

    bool camera_server_start();

    bool execute_frame_to_ply();

private:

    struct Frame_Metadata {
        uint64_t _device_frame_number; /*!< Real frame number generated from Intel Librealsense API */
        uint64_t _local_frame_number; /*!< 64-bit unsigned integer [0-1023] that refers to an "index" of a 1024 length "buffer" */
        uint64_t _timestamp; /*!< 64-bit unsigned integer timestamp */
        std::string _file_path;
        std::string metadata_to_string();
    };

    Camera_Server_Operating_Mode mode;

    rs2::context RS2_ctx;
    rs2::pointcloud RS2_pointcloud;
    rs2::points RS2_points;
    rs2::pipeline RS2_pipeline;

    std::vector<Camera_Server::Frame_Metadata> frame_buffer;
    std::mutex frame_buffer_lock;
    std::condition_variable frame_buffer_ready_cond;
    std::atomic<std::size_t> frame_buffer_index;

    class Server {
    public:
        struct sockaddr_in server_addr;
        pthread_t id;
        int64_t server_file_descriptor;
        int64_t status;

        std::vector<char> internal_buffer;

        class Network_Message {
            static constexpr const std::size_t HEADER_BEGIN = 0;
            static constexpr const std::size_t HEADER_END = 16;
        };

        typedef void (Camera_Server::* member_function_ptr)(Network_Message);

        std::map<std::string, member_function_ptr> message_map;
    };

    void CONNECT(Server::Network_Message request);

    void SEND_FRAME(Server::Network_Message request);

    bool DISCONNECT(Server::Network_Message request);

    Server camera_server_instance;

    int network_handler();

    int camera_handler();

    bool camera_server_connect();

    bool camera_server_compute_pointcloud();

    bool camera_server_export_to_ply();

};

#endif //REALSENSE_SERVER_CAMERA_SERVER_H
