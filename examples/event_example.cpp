#include "msomeip/runtime.h"
#include "msomeip/application.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <cstring>

using namespace moss::msomeip;

// Service/Event IDs
constexpr ServiceId SERVICE_ID = 0x5678;
constexpr InstanceId INSTANCE_ID = 0x0001;
constexpr EventgroupId EVENTGROUP_ID = 0x0001;
constexpr EventId EVENT_ID = 0x8001;

std::atomic<bool> g_running{true};

// Publisher (server) application
void run_publisher() {
    std::cout << "[Publisher] Starting..." << std::endl;

    Runtime::Config config;
    config.name = "publisher_app";
    config.address = "127.0.0.1";
    config.udp_port = 30500;
    config.tcp_port = 30501;

    auto runtime = Runtime::create(config);
    if (!runtime) {
        std::cerr << "[Publisher] Failed to create runtime" << std::endl;
        return;
    }

    auto app = runtime->create_application("publisher");
    app->init();
    app->start();

    // Offer service
    ServiceConfig service_config;
    service_config.service_id = SERVICE_ID;
    service_config.instance_id = INSTANCE_ID;
    service_config.major_version = 1;
    service_config.minor_version = 0;
    service_config.unreliable = true;
    service_config.unreliable_port = 30500;

    app->offer_service(service_config);

    // Offer event
    EventConfig event_config;
    event_config.service_id = SERVICE_ID;
    event_config.instance_id = INSTANCE_ID;
    event_config.event_id = EVENT_ID;
    event_config.eventgroup_id = EVENTGROUP_ID;

    app->offer_event(event_config);
    std::cout << "[Publisher] Event offered: 0x" << std::hex << EVENT_ID << std::dec << std::endl;

    // Set subscription handler
    runtime->get_service_discovery().set_subscription_handler(
        [](const SubscriptionInfo& info) {
            std::cout << "[Publisher] Subscription request for eventgroup 0x"
                      << std::hex << info.eventgroup_id << std::dec << std::endl;
            return true; // Accept subscription
        });

    std::cout << "[Publisher] Publishing events every second... Press Ctrl+C to stop" << std::endl;

    int counter = 0;
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Publish event
        std::string event_data = "Event #" + std::to_string(++counter);
        PayloadData payload(event_data.begin(), event_data.end());
        app->notify_event(EVENT_ID, payload);

        std::cout << "[Publisher] Published: " << event_data << std::endl;
    }

    app->stop_offer_event(SERVICE_ID, INSTANCE_ID, EVENT_ID);
    app->stop_offer_service(SERVICE_ID, INSTANCE_ID);
}

// Subscriber (client) application
void run_subscriber() {
    std::cout << "[Subscriber] Starting..." << std::endl;

    Runtime::Config config;
    config.name = "subscriber_app";
    config.address = "127.0.0.1";
    config.udp_port = 30502;
    config.tcp_port = 30503;

    auto runtime = Runtime::create(config);
    if (!runtime) {
        std::cerr << "[Subscriber] Failed to create runtime" << std::endl;
        return;
    }

    auto app = runtime->create_application("subscriber");
    app->init();
    app->start();

    // Set event handler
    app->set_event_handler(EVENT_ID, [](MessagePtr&& msg) {
        auto payload = msg->get_payload();
        if (payload && !payload->empty()) {
            std::string data(payload->data(), payload->data() + payload->size());
            std::cout << "[Subscriber] Received event: " << data << std::endl;
        }
    });

    // Subscribe to event
    bool subscribed = app->subscribe_event(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, EVENT_ID, 1);
    if (subscribed) {
        std::cout << "[Subscriber] Subscribed to eventgroup 0x" << std::hex << EVENTGROUP_ID << std::dec << std::endl;
    } else {
        std::cerr << "[Subscriber] Failed to subscribe" << std::endl;
        return;
    }

    std::cout << "[Subscriber] Waiting for events... Press Ctrl+C to stop" << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    app->unsubscribe_event(SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID);
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [publisher|subscriber]" << std::endl;
    std::cout << "  publisher  - Run as event publisher" << std::endl;
    std::cout << "  subscriber - Run as event subscriber" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "publisher") {
        run_publisher();
    } else if (mode == "subscriber") {
        run_subscriber();
    } else {
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
