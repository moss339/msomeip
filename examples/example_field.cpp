// Example: Field Access
// This example demonstrates a configuration service with fields
// that can be read (getter) and written (setter), with notifications.

#include "msomeip/someip.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace msomeip;

// Service IDs
constexpr ServiceId CONFIG_SERVICE = 0x3456;
constexpr InstanceId CONFIG_INSTANCE = 0x0001;

// Field IDs
constexpr FieldId FIELD_BRIGHTNESS = 0x8001;
constexpr FieldId FIELD_VOLUME = 0x8002;
constexpr FieldId FIELD_POWER_STATE = 0x8003;

// Eventgroup ID
constexpr EventgroupId EVENTGROUP_CONFIG = 0x0001;

// Configuration Service
class ConfigService : public ServiceSkeleton {
public:
    ConfigService(std::shared_ptr<Application> app)
        : ServiceSkeleton(app, CONFIG_SERVICE, CONFIG_INSTANCE) {}

    void init() override {
        // Register fields with getters and setters
        register_field(FIELD_BRIGHTNESS, EVENTGROUP_CONFIG,
            [this]() -> PayloadData { return get_brightness(); },      // getter
            [this](const PayloadData& data) -> bool { return set_brightness(data); },  // setter
            false);

        register_field(FIELD_VOLUME, EVENTGROUP_CONFIG,
            [this]() -> PayloadData { return get_volume(); },
            [this](const PayloadData& data) -> bool { return set_volume(data); },
            false);

        register_field(FIELD_POWER_STATE, EVENTGROUP_CONFIG,
            [this]() -> PayloadData { return get_power_state(); },
            [this](const PayloadData& data) -> bool { return set_power_state(data); },
            false);

        ServiceSkeleton::init();
    }

private:
    std::atomic<int32_t> brightness_{50};  // 0-100
    std::atomic<int32_t> volume_{30};      // 0-100
    std::atomic<bool> power_state_{true};

    PayloadData encode_int32(int32_t value) {
        PayloadData data;
        data.push_back(static_cast<uint8_t>(value >> 24));
        data.push_back(static_cast<uint8_t>(value >> 16));
        data.push_back(static_cast<uint8_t>(value >> 8));
        data.push_back(static_cast<uint8_t>(value));
        return data;
    }

    int32_t decode_int32(const PayloadData& data) {
        if (data.size() < 4) return 0;
        return (static_cast<int32_t>(data[0]) << 24) |
               (static_cast<int32_t>(data[1]) << 16) |
               (static_cast<int32_t>(data[2]) << 8) |
               static_cast<int32_t>(data[3]);
    }

    PayloadData get_brightness() {
        std::cout << "[Server] Getter: brightness = " << brightness_ << std::endl;
        return encode_int32(brightness_);
    }

    bool set_brightness(const PayloadData& data) {
        int32_t value = decode_int32(data);
        if (value < 0 || value > 100) {
            std::cout << "[Server] Setter: brightness " << value << " out of range" << std::endl;
            return false;
        }
        brightness_ = value;
        std::cout << "[Server] Setter: brightness set to " << value << std::endl;
        return true;
    }

    PayloadData get_volume() {
        std::cout << "[Server] Getter: volume = " << volume_ << std::endl;
        return encode_int32(volume_);
    }

    bool set_volume(const PayloadData& data) {
        int32_t value = decode_int32(data);
        if (value < 0 || value > 100) {
            std::cout << "[Server] Setter: volume " << value << " out of range" << std::endl;
            return false;
        }
        volume_ = value;
        std::cout << "[Server] Setter: volume set to " << value << std::endl;
        return true;
    }

    PayloadData get_power_state() {
        std::cout << "[Server] Getter: power = " << (power_state_ ? "ON" : "OFF") << std::endl;
        return encode_int32(power_state_ ? 1 : 0);
    }

    bool set_power_state(const PayloadData& data) {
        int32_t value = decode_int32(data);
        power_state_ = (value != 0);
        std::cout << "[Server] Setter: power set to " << (power_state_ ? "ON" : "OFF") << std::endl;
        return true;
    }
};

// Config Client
class ConfigClient : public ServiceProxy {
public:
    ConfigClient(std::shared_ptr<Application> app)
        : ServiceProxy(app, CONFIG_SERVICE, CONFIG_INSTANCE) {}

    void init() override {
        ServiceProxy::init();

        // Subscribe to field notifications
        subscribe_event(FIELD_BRIGHTNESS, EVENTGROUP_CONFIG,
            [this](MessagePtr&& msg) {
                if (msg->get_payload()) {
                    int32_t value = decode_int32(msg->get_payload()->get_data());
                    std::cout << "[Client] Notification: brightness changed to " << value << std::endl;
                }
            });

        subscribe_event(FIELD_VOLUME, EVENTGROUP_CONFIG,
            [this](MessagePtr&& msg) {
                if (msg->get_payload()) {
                    int32_t value = decode_int32(msg->get_payload()->get_data());
                    std::cout << "[Client] Notification: volume changed to " << value << std::endl;
                }
            });

        subscribe_event(FIELD_POWER_STATE, EVENTGROUP_CONFIG,
            [this](MessagePtr&& msg) {
                if (msg->get_payload()) {
                    int32_t value = decode_int32(msg->get_payload()->get_data());
                    std::cout << "[Client] Notification: power changed to "
                              << (value ? "ON" : "OFF") << std::endl;
                }
            });
    }

    std::optional<int32_t> get_brightness() {
        auto result = get_field(FIELD_BRIGHTNESS);
        if (result) {
            return decode_int32(*result);
        }
        return std::nullopt;
    }

    bool set_brightness(int32_t value) {
        return set_field(FIELD_BRIGHTNESS, encode_int32(value));
    }

    std::optional<int32_t> get_volume() {
        auto result = get_field(FIELD_VOLUME);
        if (result) {
            return decode_int32(*result);
        }
        return std::nullopt;
    }

    bool set_volume(int32_t value) {
        return set_field(FIELD_VOLUME, encode_int32(value));
    }

    std::optional<bool> get_power_state() {
        auto result = get_field(FIELD_POWER_STATE);
        if (result) {
            return decode_int32(*result) != 0;
        }
        return std::nullopt;
    }

    bool set_power_state(bool on) {
        return set_field(FIELD_POWER_STATE, encode_int32(on ? 1 : 0));
    }

private:
    PayloadData encode_int32(int32_t value) {
        PayloadData data;
        data.push_back(static_cast<uint8_t>(value >> 24));
        data.push_back(static_cast<uint8_t>(value >> 16));
        data.push_back(static_cast<uint8_t>(value >> 8));
        data.push_back(static_cast<uint8_t>(value));
        return data;
    }

    int32_t decode_int32(const PayloadData& data) {
        if (data.size() < 4) return 0;
        return (static_cast<int32_t>(data[0]) << 24) |
               (static_cast<int32_t>(data[1]) << 16) |
               (static_cast<int32_t>(data[2]) << 8) |
               static_cast<int32_t>(data[3]);
    }
};

void run_server() {
    std::cout << "Starting Configuration Service..." << std::endl;

    auto runtime = Runtime::create({"config_server", "127.0.0.1", 30509, 30490});
    if (!runtime) {
        std::cerr << "Failed to create runtime" << std::endl;
        return;
    }

    runtime->start();

    auto app = runtime->create_application("ConfigService");
    app->init();

    auto service = std::make_shared<ConfigService>(app);
    service->init();
    service->offer();

    std::cout << "Configuration Service offered. Press Enter to stop..." << std::endl;
    std::cin.get();

    service->stop_offer();
    runtime->stop();
}

void run_client() {
    std::cout << "Starting Config Client..." << std::endl;

    auto runtime = Runtime::create({"config_client", "127.0.0.1", 30510, 0});
    if (!runtime) {
        std::cerr << "Failed to create runtime" << std::endl;
        return;
    }

    runtime->start();

    auto app = runtime->create_application("ConfigClient");
    app->init();

    auto client = std::make_shared<ConfigClient>(app);
    client->init();

    // Wait for service
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n=== Field Access Demo ===" << std::endl;

    // Get initial values
    auto brightness = client->get_brightness();
    if (brightness) {
        std::cout << "Initial brightness: " << *brightness << std::endl;
    }

    auto volume = client->get_volume();
    if (volume) {
        std::cout << "Initial volume: " << *volume << std::endl;
    }

    auto power = client->get_power_state();
    if (power) {
        std::cout << "Initial power: " << (*power ? "ON" : "OFF") << std::endl;
    }

    // Set new values
    std::cout << "\nSetting new values..." << std::endl;
    client->set_brightness(75);
    client->set_volume(50);
    client->set_power_state(false);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Get updated values
    std::cout << "\nReading updated values..." << std::endl;
    brightness = client->get_brightness();
    if (brightness) {
        std::cout << "Updated brightness: " << *brightness << std::endl;
    }

    volume = client->get_volume();
    if (volume) {
        std::cout << "Updated volume: " << *volume << std::endl;
    }

    power = client->get_power_state();
    if (power) {
        std::cout << "Updated power: " << (*power ? "ON" : "OFF") << std::endl;
    }

    // Test invalid value
    std::cout << "\nTesting invalid value (brightness = 150)..." << std::endl;
    bool success = client->set_brightness(150);
    std::cout << "Set brightness result: " << (success ? "SUCCESS" : "FAILED") << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(2));
    runtime->stop();
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "server") {
        run_server();
    } else if (argc > 1 && std::string(argv[1]) == "client") {
        run_client();
    } else {
        std::cout << "Usage: " << argv[0] << " [server|client]" << std::endl;
        std::cout << "  server - Start the configuration service" << std::endl;
        std::cout << "  client - Start the configuration client" << std::endl;
    }

    return 0;
}
