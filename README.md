# SOME/IP Protocol Stack

A lightweight C++17 implementation of the SOME/IP (Scalable service-Oriented Middleware over IP) protocol stack, inspired by vsomeip.

## Features

- **SOME/IP Message Format**: Complete implementation of SOME/IP message serialization/deserialization
- **Service Discovery**: Support for finding, offering, and subscribing to services
- **Methods**: Synchronous and asynchronous remote procedure calls
- **Events**: Publish/subscribe pattern for event notifications
- **Fields**: Getter/setter/notifier pattern for field access
- **Transport**: UDP and TCP transport support
- **C++17**: Modern C++ with smart pointers, std::optional, std::future, etc.

## Project Structure

```
someip/
├── CMakeLists.txt              # Main CMake configuration
├── README.md                   # This file
├── include/someip/             # Public headers
│   ├── someip.h               # Main header (includes all)
│   ├── types.h                # Type definitions and constants
│   ├── runtime.h              # Runtime singleton
│   ├── application.h          # Application API
│   ├── service_base.h         # Service skeleton/proxy base classes
│   ├── message/
│   │   ├── message.h          # SOME/IP message
│   │   └── payload.h          # Payload helpers
│   ├── sd/
│   │   ├── service_discovery.h # Service discovery
│   │   └── service_entry.h    # SD entries and options
│   └── transport/
│       ├── udp_transport.h    # UDP transport
│       └── tcp_transport.h    # TCP transport
├── src/                        # Implementation files
│   ├── runtime.cpp
│   ├── application.cpp
│   ├── service_base.cpp
│   ├── message/
│   │   ├── message.cpp
│   │   └── payload.cpp
│   ├── sd/
│   │   ├── service_discovery.cpp
│   │   └── service_entry.cpp
│   └── transport/
│       ├── udp_transport.cpp
│       └── tcp_transport.cpp
├── tests/                      # Unit tests (Google Test)
│   ├── test_message.cpp
│   ├── test_payload.cpp
│   └── test_service_discovery.cpp
└── examples/                   # Example applications
    ├── example_method.cpp     # Method call example
    ├── example_event.cpp      # Event subscription example
    ├── example_field.cpp      # Field access example
    └── example_discovery.cpp  # Service discovery example
```

## Building

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.14+
- pthread

### Build Instructions

```bash
mkdir build
cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
make -j4
```

### Running Tests

```bash
./tests/someip_tests
```

## Usage Examples

### Method Call (Client-Server)

```cpp
// Server - Offer a calculator service
class CalculatorService : public ServiceSkeleton {
public:
    CalculatorService(std::shared_ptr<Application> app)
        : ServiceSkeleton(app, 0x1234, 0x0001) {}

    void init() override {
        register_method(0x0001, [](const PayloadData& payload) {
            // Process request
            return result_payload;
        });
        ServiceSkeleton::init();
    }
};

auto runtime = Runtime::create({"server", "127.0.0.1", 30509});
auto app = runtime->create_application("CalcService");
auto service = std::make_shared<CalculatorService>(app);
service->init();
service->offer();

// Client - Call the service
auto client_runtime = Runtime::create({"client", "127.0.0.1", 30510});
auto client_app = client_runtime->create_application("CalcClient");

auto future = client_app->send_request(
    0x1234, 0x0001, 0x0001, payload, false);
auto response = future.get();
```

### Event Subscription

```cpp
// Server - Publish events
void notify_event(EventId event, const PayloadData& payload);

// Client - Subscribe to events
app->subscribe_event(service, instance, eventgroup, event, major_version);
app->set_event_handler(event, [](MessagePtr&& msg) {
    // Handle event
});
```

### Field Access

```cpp
// Server - Offer field with getter/setter
register_field(FIELD_ID, EVENTGROUP,
    []() -> PayloadData { return get_value(); },           // getter
    [](const PayloadData& data) -> bool { return set_value(data); }  // setter
);

// Client - Get/Set field
auto value = proxy->get_field(FIELD_ID);
proxy->set_field(FIELD_ID, new_value);
```

## API Overview

### Core Classes

- **Runtime**: Main entry point, manages transport and service discovery
- **Application**: Represents a client or server application
- **ServiceSkeleton**: Base class for service implementations (servers)
- **ServiceProxy**: Base class for service clients
- **Message**: SOME/IP message representation
- **Payload**: Message payload with serialization helpers

### Key Types

- `ServiceId`, `InstanceId`, `MethodId`, `EventId`, `FieldId`: 16-bit identifiers
- `MessageType`: REQUEST, REQUEST_NO_RETURN, NOTIFICATION, RESPONSE, ERROR
- `ReturnCode`: E_OK, E_NOT_OK, E_UNKNOWN_SERVICE, etc.
- `PayloadData`: `std::vector<uint8_t>` for raw payload data

## Protocol Compliance

This implementation follows the SOME/IP specification:
- Message header format (16 bytes)
- Service Discovery message format
- Entry types: FIND_SERVICE, OFFER_SERVICE, SUBSCRIBE_EVENTGROUP, etc.
- Option types: IPv4 Endpoint, IPv4 Multicast

## Limitations

This is a minimal implementation for educational and prototyping purposes:
- Limited SOME/IP-TP (Transport Protocol) support
- Basic service discovery (no full state machines)
- IPv6 not fully implemented
- No security features
- Single-threaded message handling

## License

MIT License - See LICENSE file for details.

## References

- [AUTOSAR SOME/IP Specification](https://www.autosar.org/standards/foundation/)
- [vsomeip](https://github.com/COVESA/vsomeip) - The reference implementation
