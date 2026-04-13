#include "msomeip/runtime.h"
#include "msomeip/application.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

using namespace moss::msomeip;

// Service IDs
constexpr ServiceId SERVICE_ID = 0x1234;
constexpr InstanceId INSTANCE_ID = 0x0001;
constexpr MethodId METHOD_ID = 0x0001;

// Server application
void run_server() {
    std::cout << "[Server] Starting..." << std::endl;

    // Create runtime
    Runtime::Config config;
    config.name = "server_app";
    config.address = "127.0.0.1";
    config.udp_port = 30490;
    config.tcp_port = 30491;

    auto runtime = Runtime::create(config);
    if (!runtime) {
        std::cerr << "[Server] Failed to create runtime" << std::endl;
        return;
    }

    auto app = runtime->create_application("server");
    app->init();
    app->start();

    // Offer service
    ServiceConfig service_config;
    service_config.service_id = SERVICE_ID;
    service_config.instance_id = INSTANCE_ID;
    service_config.major_version = 1;
    service_config.minor_version = 0;
    service_config.unreliable = true;
    service_config.unreliable_port = 30490;

    app->offer_service(service_config);
    std::cout << "[Server] Service offered: 0x" << std::hex << SERVICE_ID << std::dec << std::endl;

    // Register method handler
    app->register_method_handler(METHOD_ID, [](MessagePtr&& msg) {
        std::cout << "[Server] Received request from client 0x"
                  << std::hex << msg->get_client_id() << std::dec << std::endl;

        // Print request payload
        auto payload = msg->get_payload();
        if (payload && !payload->empty()) {
            std::string data(payload->data(), payload->data() + payload->size());
            std::cout << "[Server] Request data: " << data << std::endl;
        }

        // Send response
        std::string response_str = "Hello from server!";
        PayloadData response_data(response_str.begin(), response_str.end());
        app->send_response(msg, response_data);
    });

    std::cout << "[Server] Running... Press Ctrl+C to stop" << std::endl;

    // Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// Client application
void run_client() {
    std::cout << "[Client] Starting..." << std::endl;

    // Create runtime
    Runtime::Config config;
    config.name = "client_app";
    config.address = "127.0.0.1";
    config.udp_port = 30492;
    config.tcp_port = 30493;

    auto runtime = Runtime::create(config);
    if (!runtime) {
        std::cerr << "[Client] Failed to create runtime" << std::endl;
        return;
    }

    auto app = runtime->create_application("client");
    app->init();
    app->start();

    // Set availability handler
    app->set_availability_handler([](ServiceId service, InstanceId instance, bool available) {
        std::cout << "[Client] Service 0x" << std::hex << service << std::dec
                  << " instance 0x" << std::hex << instance << std::dec
                  << " is " << (available ? "available" : "unavailable") << std::endl;
    });

    // Find service
    runtime->get_service_discovery().find_service(SERVICE_ID, INSTANCE_ID);

    // Wait for service to be available
    std::cout << "[Client] Waiting for service..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Send request
    std::string request_str = "Hello from client!";
    PayloadData request_data(request_str.begin(), request_str.end());

    std::cout << "[Client] Sending request..." << std::endl;
    auto future = app->send_request(SERVICE_ID, INSTANCE_ID, METHOD_ID, request_data, false);

    // Wait for response
    auto status = future.wait_for(std::chrono::seconds(5));
    if (status == std::future_status::ready) {
        auto response = future.get();
        if (response) {
            auto payload = response->get_payload();
            if (payload && !payload->empty()) {
                std::string data(payload->data(), payload->data() + payload->size());
                std::cout << "[Client] Received response: " << data << std::endl;
            }
        }
    } else {
        std::cout << "[Client] Request timeout" << std::endl;
    }

    std::cout << "[Client] Done" << std::endl;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [server|client]" << std::endl;
    std::cout << "  server  - Run as server" << std::endl;
    std::cout << "  client  - Run as client" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "server") {
        run_server();
    } else if (mode == "client") {
        run_client();
    } else {
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
