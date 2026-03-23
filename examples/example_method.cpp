// Example: Simple Method Call
// This example demonstrates a server offering a calculator service
// and a client calling methods on it.

#include "msomeip/someip.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace msomeip;

// Service IDs
constexpr ServiceId CALCULATOR_SERVICE = 0x1234;
constexpr InstanceId CALCULATOR_INSTANCE = 0x0001;

// Method IDs
constexpr MethodId METHOD_ADD = 0x0001;
constexpr MethodId METHOD_SUBTRACT = 0x0002;
constexpr MethodId METHOD_MULTIPLY = 0x0003;
constexpr MethodId METHOD_DIVIDE = 0x0004;

// Simple payload encoding: [4 bytes operand1][4 bytes operand2]
PayloadData encode_request(int32_t a, int32_t b) {
    PayloadData data;
    data.reserve(8);
    auto append_int32 = [&data](int32_t value) {
        data.push_back(static_cast<uint8_t>(value >> 24));
        data.push_back(static_cast<uint8_t>(value >> 16));
        data.push_back(static_cast<uint8_t>(value >> 8));
        data.push_back(static_cast<uint8_t>(value));
    };
    append_int32(a);
    append_int32(b);
    return data;
}

std::optional<int32_t> decode_response(const PayloadData& data) {
    if (data.size() < 4) return std::nullopt;
    int32_t result = (static_cast<int32_t>(data[0]) << 24) |
                     (static_cast<int32_t>(data[1]) << 16) |
                     (static_cast<int32_t>(data[2]) << 8) |
                     static_cast<int32_t>(data[3]);
    return result;
}

// Server-side calculator service
class CalculatorService : public ServiceSkeleton {
public:
    CalculatorService(std::shared_ptr<Application> app)
        : ServiceSkeleton(app, CALCULATOR_SERVICE, CALCULATOR_INSTANCE) {}

    void init() override {
        // Register method handlers
        register_method(METHOD_ADD, [this](const PayloadData& payload) {
            return handle_add(payload);
        });

        register_method(METHOD_SUBTRACT, [this](const PayloadData& payload) {
            return handle_subtract(payload);
        });

        register_method(METHOD_MULTIPLY, [this](const PayloadData& payload) {
            return handle_multiply(payload);
        });

        register_method(METHOD_DIVIDE, [this](const PayloadData& payload) {
            return handle_divide(payload);
        });

        ServiceSkeleton::init();
    }

private:
    std::pair<int32_t, int32_t> decode_operands(const PayloadData& payload) {
        if (payload.size() < 8) return {0, 0};
        int32_t a = (static_cast<int32_t>(payload[0]) << 24) |
                    (static_cast<int32_t>(payload[1]) << 16) |
                    (static_cast<int32_t>(payload[2]) << 8) |
                    static_cast<int32_t>(payload[3]);
        int32_t b = (static_cast<int32_t>(payload[4]) << 24) |
                    (static_cast<int32_t>(payload[5]) << 16) |
                    (static_cast<int32_t>(payload[6]) << 8) |
                    static_cast<int32_t>(payload[7]);
        return {a, b};
    }

    PayloadData encode_result(int32_t result) {
        PayloadData data;
        data.push_back(static_cast<uint8_t>(result >> 24));
        data.push_back(static_cast<uint8_t>(result >> 16));
        data.push_back(static_cast<uint8_t>(result >> 8));
        data.push_back(static_cast<uint8_t>(result));
        return data;
    }

    PayloadData handle_add(const PayloadData& payload) {
        auto [a, b] = decode_operands(payload);
        std::cout << "[Server] Adding " << a << " + " << b << std::endl;
        return encode_result(a + b);
    }

    PayloadData handle_subtract(const PayloadData& payload) {
        auto [a, b] = decode_operands(payload);
        std::cout << "[Server] Subtracting " << a << " - " << b << std::endl;
        return encode_result(a - b);
    }

    PayloadData handle_multiply(const PayloadData& payload) {
        auto [a, b] = decode_operands(payload);
        std::cout << "[Server] Multiplying " << a << " * " << b << std::endl;
        return encode_result(a * b);
    }

    PayloadData handle_divide(const PayloadData& payload) {
        auto [a, b] = decode_operands(payload);
        std::cout << "[Server] Dividing " << a << " / " << b << std::endl;
        if (b == 0) {
            return encode_result(0); // Error case
        }
        return encode_result(a / b);
    }
};

// Client-side calculator proxy
class CalculatorClient : public ServiceProxy {
public:
    CalculatorClient(std::shared_ptr<Application> app)
        : ServiceProxy(app, CALCULATOR_SERVICE, CALCULATOR_INSTANCE) {}

    std::optional<int32_t> add(int32_t a, int32_t b) {
        auto result = call_method(METHOD_ADD, encode_request(a, b));
        if (result) {
            return decode_response(*result);
        }
        return std::nullopt;
    }

    std::optional<int32_t> subtract(int32_t a, int32_t b) {
        auto result = call_method(METHOD_SUBTRACT, encode_request(a, b));
        if (result) {
            return decode_response(*result);
        }
        return std::nullopt;
    }

    std::optional<int32_t> multiply(int32_t a, int32_t b) {
        auto result = call_method(METHOD_MULTIPLY, encode_request(a, b));
        if (result) {
            return decode_response(*result);
        }
        return std::nullopt;
    }

    std::optional<int32_t> divide(int32_t a, int32_t b) {
        auto result = call_method(METHOD_DIVIDE, encode_request(a, b));
        if (result) {
            return decode_response(*result);
        }
        return std::nullopt;
    }
};

void run_server() {
    std::cout << "Starting Calculator Server..." << std::endl;

    auto runtime = Runtime::create({"calculator_server", "127.0.0.1", 30509, 30490});
    if (!runtime) {
        std::cerr << "Failed to create runtime" << std::endl;
        return;
    }

    runtime->start();

    auto app = runtime->create_application("CalculatorService");
    app->init();

    auto service = std::make_shared<CalculatorService>(app);
    service->init();
    service->offer();

    std::cout << "Calculator Service offered. Press Enter to stop..." << std::endl;
    std::cin.get();

    service->stop_offer();
    runtime->stop();
}

void run_client() {
    std::cout << "Starting Calculator Client..." << std::endl;

    auto runtime = Runtime::create({"calculator_client", "127.0.0.1", 30510, 0});
    if (!runtime) {
        std::cerr << "Failed to create runtime" << std::endl;
        return;
    }

    runtime->start();

    auto app = runtime->create_application("CalculatorClient");
    app->init();

    auto client = std::make_shared<CalculatorClient>(app);
    client->init();

    // Wait for service to be available
    std::cout << "Waiting for service..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Perform calculations
    std::cout << "\n=== Calculator Demo ===" << std::endl;

    auto result = client->add(10, 5);
    if (result) {
        std::cout << "10 + 5 = " << *result << std::endl;
    }

    result = client->subtract(10, 5);
    if (result) {
        std::cout << "10 - 5 = " << *result << std::endl;
    }

    result = client->multiply(10, 5);
    if (result) {
        std::cout << "10 * 5 = " << *result << std::endl;
    }

    result = client->divide(10, 5);
    if (result) {
        std::cout << "10 / 5 = " << *result << std::endl;
    }

    runtime->stop();
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "server") {
        run_server();
    } else if (argc > 1 && std::string(argv[1]) == "client") {
        run_client();
    } else {
        std::cout << "Usage: " << argv[0] << " [server|client]" << std::endl;
        std::cout << "  server - Start the calculator service" << std::endl;
        std::cout << "  client - Start the calculator client" << std::endl;
    }

    return 0;
}
