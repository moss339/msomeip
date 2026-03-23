// Example: Service Discovery
// This example demonstrates service discovery functionality where
// multiple services are offered and discovered dynamically.

#include "msomeip/someip.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

using namespace msomeip;

// Multiple service types
constexpr ServiceId SERVICE_TYPE_CAMERA = 0x4001;
constexpr ServiceId SERVICE_TYPE_GPS = 0x4002;
constexpr ServiceId SERVICE_TYPE_AUDIO = 0x4003;

// Service instances
constexpr InstanceId INSTANCE_1 = 0x0001;
constexpr InstanceId INSTANCE_2 = 0x0002;

// Base service class
class SimpleService : public ServiceSkeleton {
public:
    SimpleService(std::shared_ptr<Application> app, ServiceId service, InstanceId instance)
        : ServiceSkeleton(app, service, instance) {}

    void init() override {
        ServiceSkeleton::init();
    }
};

// Discovery Monitor - discovers and monitors services
class DiscoveryMonitor {
public:
    DiscoveryMonitor(std::shared_ptr<Application> app)
        : app_(app) {}

    void init() {
        // Setup availability handler
        app_->set_availability_handler(
            [this](ServiceId service, InstanceId instance, bool available) {
                on_service_changed(service, instance, available);
            });

        // Find all services we're interested in
        app_->init();

        // Start discovery for each service type
        auto& sd = app_->get_runtime()->get_service_discovery();
        sd.find_service(SERVICE_TYPE_CAMERA);
        sd.find_service(SERVICE_TYPE_GPS);
        sd.find_service(SERVICE_TYPE_AUDIO);
    }

    void print_discovered_services() {
        std::cout << "\n=== Discovered Services ===" << std::endl;

        auto& sd = app_->get_runtime()->get_service_discovery();
        auto services = sd.get_discovered_services();

        if (services.empty()) {
            std::cout << "No services discovered yet." << std::endl;
        } else {
            for (const auto& info : services) {
                std::cout << "Service 0x" << std::hex << info.service_id
                          << std::dec << ", Instance 0x" << info.instance_id
                          << " (v" << (int)info.major_version << "." << info.minor_version << ")"
                          << " TTL: " << info.ttl << "s"
                          << std::endl;

                std::cout << "  Endpoints:" << std::endl;
                for (const auto& ep : info.endpoints) {
                    std::cout << "    " << ep.address << ":" << ep.port
                              << " (" << (ep.protocol == IpProtocol::UDP ? "UDP" : "TCP") << ")"
                              << std::endl;
                }
            }
        }
        std::cout << std::endl;
    }

private:
    void on_service_changed(ServiceId service, InstanceId instance, bool available) {
        std::cout << "[Discovery] Service 0x" << std::hex << service
                  << ", Instance 0x" << instance << std::dec
                  << " is now " << (available ? "AVAILABLE" : "UNAVAILABLE")
                  << std::endl;
    }

    std::shared_ptr<Application> app_;
};

void run_service_provider() {
    std::cout << "Starting Service Provider (offering multiple services)..." << std::endl;

    auto runtime = Runtime::create({"service_provider", "127.0.0.1", 30509, 30490});
    if (!runtime) {
        std::cerr << "Failed to create runtime" << std::endl;
        return;
    }

    runtime->start();

    auto app = runtime->create_application("ServiceProvider");
    app->init();

    // Create and offer multiple services
    std::vector<std::shared_ptr<SimpleService>> services;

    // Camera Service 1
    auto camera1 = std::make_shared<SimpleService>(app, SERVICE_TYPE_CAMERA, INSTANCE_1);
    camera1->init();
    camera1->offer();
    services.push_back(camera1);
    std::cout << "Offered Camera Service (0x" << std::hex << SERVICE_TYPE_CAMERA
              << ", instance 0x" << INSTANCE_1 << std::dec << ")" << std::endl;

    // Camera Service 2
    auto camera2 = std::make_shared<SimpleService>(app, SERVICE_TYPE_CAMERA, INSTANCE_2);
    camera2->init();
    camera2->offer();
    services.push_back(camera2);
    std::cout << "Offered Camera Service (0x" << std::hex << SERVICE_TYPE_CAMERA
              << ", instance 0x" << INSTANCE_2 << std::dec << ")" << std::endl;

    // GPS Service
    auto gps = std::make_shared<SimpleService>(app, SERVICE_TYPE_GPS, INSTANCE_1);
    gps->init();
    gps->offer();
    services.push_back(gps);
    std::cout << "Offered GPS Service (0x" << std::hex << SERVICE_TYPE_GPS
              << ", instance 0x" << INSTANCE_1 << std::dec << ")" << std::endl;

    // Audio Service
    auto audio = std::make_shared<SimpleService>(app, SERVICE_TYPE_AUDIO, INSTANCE_1);
    audio->init();
    audio->offer();
    services.push_back(audio);
    std::cout << "Offered Audio Service (0x" << std::hex << SERVICE_TYPE_AUDIO
              << ", instance 0x" << INSTANCE_1 << std::dec << ")" << std::endl;

    std::cout << "\nAll services offered. Press Enter to stop offering services one by one..."
              << std::endl;
    std::cin.get();

    // Stop offering one by one to demonstrate unavailability notifications
    for (auto& service : services) {
        std::cout << "Stopping offer for service..." << std::endl;
        service->stop_offer();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    runtime->stop();
}

void run_discovery_client() {
    std::cout << "Starting Discovery Client..." << std::endl;

    auto runtime = Runtime::create({"discovery_client", "127.0.0.1", 30510, 0});
    if (!runtime) {
        std::cerr << "Failed to create runtime" << std::endl;
        return;
    }

    runtime->start();

    auto app = runtime->create_application("DiscoveryClient");

    auto monitor = std::make_shared<DiscoveryMonitor>(app);
    monitor->init();

    // Periodically print discovered services
    for (int i = 0; i < 60; ++i) {
        monitor->print_discovered_services();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    runtime->stop();
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "provider") {
        run_service_provider();
    } else if (argc > 1 && std::string(argv[1]) == "client") {
        run_discovery_client();
    } else {
        std::cout << "Usage: " << argv[0] << " [provider|client]" << std::endl;
        std::cout << "  provider - Start the service provider (offers multiple services)" << std::endl;
        std::cout << "  client   - Start the discovery client (discovers services)" << std::endl;
    }

    return 0;
}
