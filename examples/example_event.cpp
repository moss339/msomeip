// Example: Event Subscription
// This example demonstrates a temperature sensor that publishes events
// and clients that subscribe to receive temperature updates.

#include "msomeip/someip.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>

using namespace msomeip;

// Service IDs
constexpr ServiceId TEMPERATURE_SERVICE = 0x2345;
constexpr InstanceId TEMPERATURE_INSTANCE = 0x0001;

// Event IDs
constexpr EventId EVENT_TEMPERATURE_UPDATE = 0x8001;
constexpr EventId EVENT_HUMIDITY_UPDATE = 0x8002;

// Eventgroup ID
constexpr EventgroupId EVENTGROUP_SENSOR = 0x0001;

// Payload encoding for temperature: [4 bytes temperature * 100 (fixed point)]
PayloadData encode_temperature(float temp) {
    int32_t temp_fixed = static_cast<int32_t>(temp * 100);
    PayloadData data;
    data.push_back(static_cast<uint8_t>(temp_fixed >> 24));
    data.push_back(static_cast<uint8_t>(temp_fixed >> 16));
    data.push_back(static_cast<uint8_t>(temp_fixed >> 8));
    data.push_back(static_cast<uint8_t>(temp_fixed));
    return data;
}

PayloadData encode_humidity(float humidity) {
    int32_t hum_fixed = static_cast<int32_t>(humidity * 100);
    PayloadData data;
    data.push_back(static_cast<uint8_t>(hum_fixed >> 24));
    data.push_back(static_cast<uint8_t>(hum_fixed >> 16));
    data.push_back(static_cast<uint8_t>(hum_fixed >> 8));
    data.push_back(static_cast<uint8_t>(hum_fixed));
    return data;
}

float decode_sensor_value(const PayloadData& data) {
    if (data.size() < 4) return 0.0f;
    int32_t value = (static_cast<int32_t>(data[0]) << 24) |
                    (static_cast<int32_t>(data[1]) << 16) |
                    (static_cast<int32_t>(data[2]) << 8) |
                    static_cast<int32_t>(data[3]);
    return value / 100.0f;
}

// Temperature Sensor Service
class TemperatureSensor : public ServiceSkeleton {
public:
    TemperatureSensor(std::shared_ptr<Application> app)
        : ServiceSkeleton(app, TEMPERATURE_SERVICE, TEMPERATURE_INSTANCE)
        , running_(false) {}

    void init() override {
        // Register events
        register_event(EVENT_TEMPERATURE_UPDATE, EVENTGROUP_SENSOR, false);
        register_event(EVENT_HUMIDITY_UPDATE, EVENTGROUP_SENSOR, false);

        ServiceSkeleton::init();
    }

    void start_publishing() {
        running_ = true;
        publish_thread_ = std::thread([this]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> temp_dist(20.0f, 30.0f);
            std::uniform_real_distribution<float> hum_dist(40.0f, 70.0f);

            while (running_) {
                float temp = temp_dist(gen);
                float humidity = hum_dist(gen);

                std::cout << "[Sensor] Temperature: " << temp
                          << "C, Humidity: " << humidity << "%" << std::endl;

                notify_event(EVENT_TEMPERATURE_UPDATE, encode_temperature(temp));
                notify_event(EVENT_HUMIDITY_UPDATE, encode_humidity(humidity));

                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        });
    }

    void stop_publishing() {
        running_ = false;
        if (publish_thread_.joinable()) {
            publish_thread_.join();
        }
    }

private:
    std::atomic<bool> running_;
    std::thread publish_thread_;
};

// Sensor Client
class SensorClient : public ServiceProxy {
public:
    SensorClient(std::shared_ptr<Application> app)
        : ServiceProxy(app, TEMPERATURE_SERVICE, TEMPERATURE_INSTANCE) {}

    void init() override {
        ServiceProxy::init();

        // Subscribe to events
        subscribe_event(EVENT_TEMPERATURE_UPDATE, EVENTGROUP_SENSOR,
            [this](MessagePtr&& msg) {
                if (msg->get_payload()) {
                    float temp = decode_sensor_value(msg->get_payload()->get_data());
                    std::cout << "[Client] Received temperature: " << temp << "°C" << std::endl;
                }
            });

        subscribe_event(EVENT_HUMIDITY_UPDATE, EVENTGROUP_SENSOR,
            [this](MessagePtr&& msg) {
                if (msg->get_payload()) {
                    float humidity = decode_sensor_value(msg->get_payload()->get_data());
                    std::cout << "[Client] Received humidity: " << humidity << "%" << std::endl;
                }
            });
    }
};

void run_sensor() {
    std::cout << "Starting Temperature Sensor Service..." << std::endl;

    auto runtime = Runtime::create({"sensor_server", "127.0.0.1", 30509, 30490});
    if (!runtime) {
        std::cerr << "Failed to create runtime" << std::endl;
        return;
    }

    runtime->start();

    auto app = runtime->create_application("TemperatureSensor");
    app->init();

    auto sensor = std::make_shared<TemperatureSensor>(app);
    sensor->init();
    sensor->offer();
    sensor->start_publishing();

    std::cout << "Temperature Sensor publishing events. Press Enter to stop..." << std::endl;
    std::cin.get();

    sensor->stop_publishing();
    sensor->stop_offer();
    runtime->stop();
}

void run_client() {
    std::cout << "Starting Sensor Client..." << std::endl;

    auto runtime = Runtime::create({"sensor_client", "127.0.0.1", 30510, 0});
    if (!runtime) {
        std::cerr << "Failed to create runtime" << std::endl;
        return;
    }

    runtime->start();

    auto app = runtime->create_application("SensorClient");
    app->init();

    auto client = std::make_shared<SensorClient>(app);
    client->init();

    std::cout << "Client subscribed to sensor events. Listening for 30 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(30));

    runtime->stop();
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "sensor") {
        run_sensor();
    } else if (argc > 1 && std::string(argv[1]) == "client") {
        run_client();
    } else {
        std::cout << "Usage: " << argv[0] << " [sensor|client]" << std::endl;
        std::cout << "  sensor - Start the temperature sensor service" << std::endl;
        std::cout << "  client - Start the sensor client" << std::endl;
    }

    return 0;
}
