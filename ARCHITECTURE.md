# SOME/IP Protocol Stack - 技术架构文档

## 1. 项目概述

这是一个轻量级 C++17 实现的 SOME/IP (Scalable service-Oriented Middleware over IP) 协议栈，灵感来源于 vsomeip。

### 1.1 核心特性
- **SOME/IP 消息格式**: 完整实现消息序列化/反序列化
- **服务发现**: 支持服务查找、提供和订阅
- **方法调用**: 同步和异步远程过程调用
- **事件**: 发布/订阅模式的事件通知
- **字段**: Getter/Setter/Notifier 模式的字段访问
- **传输层**: UDP 和 TCP 传输支持
- **C++17**: 使用智能指针、std::optional、std::future 等现代 C++ 特性

---

## 2. 系统架构

### 2.1 分层架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                         应用层 (Application)                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │   Service    │  │   Service    │  │      ServiceProxy    │  │
│  │  Skeleton    │  │   Proxy      │  │    (客户端代理)       │  │
│  │   (服务端)    │  │   (客户端)    │  │                      │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                      运行时层 (Runtime)                          │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    Application                          │   │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────┐  │   │
│  │  │ Method管理  │  │ Event管理   │  │ Field管理       │  │   │
│  │  └─────────────┘  └─────────────┘  └─────────────────┘  │   │
│  └─────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────┤
│                      服务发现层 (Service Discovery)               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │   Service    │  │  Eventgroup  │  │   Subscription       │  │
│  │   Registry   │  │    Registry  │  │    Manager           │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│                      传输层 (Transport)                          │
│  ┌──────────────────┐  ┌──────────────────┐                    │
│  │  UDP Transport   │  │  TCP Transport   │                    │
│  │  (不可靠传输)     │  │  (可靠传输)       │                    │
│  └──────────────────┘  └──────────────────┘                    │
├─────────────────────────────────────────────────────────────────┤
│                      消息层 (Message)                            │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              SOME/IP Message (16 bytes Header)          │   │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────┐   │   │
│  │  │Msg ID   │ │ Length  │ │Req ID   │ │Ver/Type/Code│   │   │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 核心组件关系图

```
┌────────────────────────────────────────────────────────────────────────┐
│                              Runtime                                    │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │  - 单例模式管理                                                  │  │
│  │  - 创建 Application                                              │  │
│  │  - 管理 Transport 和 ServiceDiscovery                            │  │
│  └─────────────────────────────────────────────────────────────────┘  │
│          │                          │                                  │
│          ▼                          ▼                                  │
│  ┌─────────────────┐    ┌──────────────────────┐                      │
│  │  UdpTransport   │    │ ServiceDiscovery     │                      │
│  │  TcpTransport   │    │  - 服务注册表         │                      │
│  │                 │    │  - 订阅管理          │                      │
│  └─────────────────┘    │  - TTL 管理          │                      │
│                         └──────────────────────┘                      │
└────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                           Application                                   │
│  ┌─────────────────────────────────────────────────────────────────┐  │
│  │  - 消息路由                                                      │  │
│  │  - Pending Request 管理                                          │  │
│  │  - Method/Event/Field 处理器注册                                 │  │
│  └─────────────────────────────────────────────────────────────────┘  │
│          │                    │                    │                  │
│          ▼                    ▼                    ▼                  │
│  ┌───────────────┐  ┌────────────────┐  ┌──────────────────────┐     │
│  │ServiceSkeleton│  │ ServiceProxy   │  │  Message Handlers    │     │
│  │   (服务端)     │  │   (客户端)      │  │                      │     │
│  └───────────────┘  └────────────────┘  └──────────────────────┘     │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 3. 设计模式

### 3.1 单例模式 (Singleton) - Runtime

```cpp
class Runtime : public std::enable_shared_from_this<Runtime> {
public:
    static std::shared_ptr<Runtime> get();           // 获取默认实例
    static std::shared_ptr<Runtime> create(const Config& config);  // 创建新实例
    static void set_default(const Config& config);   // 设置默认实例

private:
    static std::weak_ptr<Runtime> default_runtime_;  // 弱引用避免循环
    static std::mutex runtime_mutex_;                // 线程安全
};
```

**用途**: 管理全局运行时环境，提供统一的协议栈入口。

### 3.2 工厂模式 (Factory) - Message

```cpp
class Message {
public:
    static MessagePtr create_request(ServiceId service, InstanceId instance, ...);
    static MessagePtr create_notification(ServiceId service, InstanceId instance, ...);
    static MessagePtr create_response(const Message& request);
    static MessagePtr create_error_response(const Message& request, ReturnCode code);
    static std::optional<MessagePtr> deserialize(const uint8_t* data, size_t length);
};
```

**用途**: 统一创建不同类型的 SOME/IP 消息，隐藏构造细节。

### 3.3 代理模式 (Proxy) - ServiceProxy / ServiceSkeleton

```cpp
// 服务端骨架
class ServiceSkeleton : public std::enable_shared_from_this<ServiceSkeleton> {
    void register_method(MethodId method, Handler handler);
    void notify_event(EventId event, const PayloadData& payload);
};

// 客户端代理
class ServiceProxy : public std::enable_shared_from_this<ServiceProxy> {
    std::optional<PayloadData> call_method(MethodId method, const PayloadData& payload);
    bool subscribe_event(EventId event, EventgroupId eventgroup, Handler handler);
};
```

**用途**: 分离客户端和服务端接口，提供类型安全的服务调用。

### 3.4 观察者模式 (Observer) - Event/Subscription

```cpp
// 服务端发布事件
void notify_event(EventId event, const PayloadData& payload);

// 客户端订阅事件
bool subscribe_event(EventId event, EventgroupId eventgroup, MessageHandler handler);
```

**用途**: 实现发布-订阅模式的事件通知机制。

### 3.5 回调模式 (Callback) - 异步处理

```cpp
using MessageHandler = std::function<void(MessagePtr&&)>;
using AvailabilityHandler = std::function<void(ServiceId, InstanceId, bool)>;
using SubscriptionHandler = std::function<bool(const SubscriptionInfo&)>;
```

**用途**: 实现异步消息处理和事件驱动编程。

### 3.6 RAII 模式 (资源管理)

```cpp
class Application {
    Application(const Application&) = delete;  // 禁止拷贝
    Application& operator=(const Application&) = delete;
    // 使用智能指针管理生命周期
};
```

**用途**: 确保资源正确释放，避免内存泄漏。

---

## 4. 消息格式详解

### 4.1 SOME/IP 消息头结构

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Message ID (32 bits)                   |
|                   Service ID (16 bits)                        |
|                        Method ID (16 bits)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Length (32 bits)                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Request ID (32 bits)                    |
|                    Client ID (16 bits)                        |
|                       Session ID (16 bits)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Protocol Ver  | Interface Ver |  Message Type |  Return Code  |
|   (8 bits)    |   (8 bits)    |   (8 bits)    |   (8 bits)    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                         Payload (variable)                    |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 4.2 消息类型 (MessageType)

| 值   | 类型                  | 说明                     |
|------|----------------------|--------------------------|
| 0x00 | REQUEST              | 请求消息                 |
| 0x01 | REQUEST_NO_RETURN    | 请求无返回(fire-and-forget) |
| 0x02 | NOTIFICATION         | 事件通知                 |
| 0x80 | RESPONSE             | 响应消息                 |
| 0x81 | ERROR                | 错误响应                 |

### 4.3 返回码 (ReturnCode)

| 值   | 返回码                  | 说明                     |
|------|------------------------|--------------------------|
| 0x00 | E_OK                   | 成功                     |
| 0x01 | E_NOT_OK               | 一般错误                 |
| 0x02 | E_UNKNOWN_SERVICE      | 未知服务                 |
| 0x03 | E_UNKNOWN_METHOD       | 未知方法                 |
| 0x04 | E_NOT_READY            | 服务未就绪               |
| 0x05 | E_NOT_REACHABLE        | 不可达                   |
| 0x06 | E_TIMEOUT              | 超时                     |

---

## 5. 时序图

### 5.1 服务发现时序图

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Client  │     │  Server  │     │   SD     │     │ Network  │
└────┬─────┘     └────┬─────┘     └────┬─────┘     └────┬─────┘
     │                │                │                │
     │                │   offer_service│                │
     │                │───────────────▶│                │
     │                │                │                │
     │ find_service   │                │                │
     │───────────────▶│                │                │
     │                │                │                │
     │                │                │ OFFER_SERVICE  │
     │                │◀───────────────│───────────────▶│
     │                │                │                │
     │ on_service_available            │                │
     │◀───────────────│                │                │
     │                │                │                │
```

### 5.2 方法调用时序图 (同步)

```
┌──────────────┐              ┌──────────────┐              ┌──────────────┐
│ Client Proxy │              │   Runtime    │              │   Server     │
└──────┬───────┘              └──────┬───────┘              └──────┬───────┘
       │                             │                             │
       │ call_method(METHOD_ADD)     │                             │
       │────────────────────────────▶│                             │
       │                             │                             │
       │                             │ send_request()              │
       │                             │────────────────────────────▶│
       │                             │                             │
       │                             │        handle_method_call()
       │                             │◀────────────────────────────│
       │                             │                             │
       │                             │ send_response()             │
       │                             │◀────────────────────────────│
       │                             │                             │
       │ resolve future with result  │                             │
       │◀────────────────────────────│                             │
       │                             │                             │
```

### 5.3 事件订阅时序图

```
┌──────────────┐              ┌──────────────┐              ┌──────────────┐
│    Client    │              │   Service    │              │   Server     │
│    Proxy     │              │  Discovery   │              │  Skeleton    │
└──────┬───────┘              └──────┬───────┘              └──────┬───────┘
       │                             │                             │
       │ subscribe_event()           │                             │
       │────────────────────────────▶│                             │
       │                             │                             │
       │                             │ SUBSCRIBE_EVENTGROUP        │
       │                             │────────────────────────────▶│
       │                             │                             │
       │                             │ SUBSCRIBE_EVENTGROUP_ACK    │
       │                             │◀────────────────────────────│
       │                             │                             │
       │ subscription confirmed      │                             │
       │◀────────────────────────────│                             │
       │                             │                             │
       │                             │                             │ notify_event()
       │◀──────────────────────────────────────────────────────────│
       │                             │                             │
```

### 5.4 服务启动完整时序图

```
┌──────────┐  ┌──────────┐  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐
│   App    │  │ Runtime  │  │  Transport  │  │      SD      │  │   Service    │
└────┬─────┘  └────┬─────┘  └──────┬──────┘  └──────┬───────┘  └──────┬───────┘
     │             │               │                │                 │
     │ create()    │               │                │                 │
     │────────────▶│               │                │                 │
     │             │ init()        │                │                 │
     │             │──────────────▶│                │                 │
     │             │               │ listen()       │                 │
     │             │               │───────────────▶│                 │
     │             │               │                │                 │
     │             │ init()        │                │                 │
     │             │───────────────────────────────▶│                 │
     │             │               │                │                 │
     │             │ start()       │                │                 │
     │             │──────────────▶│                │                 │
     │             │               │ start()        │                 │
     │             │               │───────────────▶│                 │
     │             │               │                │                 │
     │             │ start()       │                │                 │
     │             │───────────────────────────────▶│                 │
     │             │               │                │                 │
     │ init()      │               │                │                 │
     │────────────▶│               │                │                 │
     │             │               │                │                 │
     │             │               │                │    offer()      │
     │             │               │                │◀────────────────│
     │             │               │                │                 │
     │             │               │                │ offer_service() │
     │             │               │                │───────────────▶│
     │             │               │                │                 │
```

---

## 6. 关键类图

```
┌────────────────────────────────────────────────────────────────────────────┐
│                              Runtime                                        │
├────────────────────────────────────────────────────────────────────────────┤
│ - config_: Config                                                           │
│ - client_id_: ClientId                                                      │
│ - udp_transport_: unique_ptr<UdpTransport>                                  │
│ - tcp_transport_: unique_ptr<TcpTransport>                                  │
│ - service_discovery_: unique_ptr<ServiceDiscovery>                          │
│ - service_handlers_: unordered_map<ServiceIdTuple, MessageHandler>          │
├────────────────────────────────────────────────────────────────────────────┤
│ + get(): shared_ptr<Runtime>                                                │
│ + create(config): shared_ptr<Runtime>                                       │
│ + init(): bool                                                              │
│ + start() / stop()                                                          │
│ + create_application(name): shared_ptr<Application>                         │
│ + send_message(msg, addr, port, reliable): bool                             │
│ + register_service_handler(svc, inst, handler)                              │
└────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      │ creates
                                      ▼
┌────────────────────────────────────────────────────────────────────────────┐
│                            Application                                      │
├────────────────────────────────────────────────────────────────────────────┤
│ - runtime_: shared_ptr<Runtime>                                             │
│ - name_: string                                                             │
│ - session_id_: SessionId                                                    │
│ - pending_requests_: unordered_map<RequestId, shared_ptr<PendingRequest>>   │
│ - method_handlers_: unordered_map<MethodId, MessageHandler>                 │
│ - event_handlers_: unordered_map<EventId, MessageHandler>                   │
├────────────────────────────────────────────────────────────────────────────┤
│ + init() / start() / stop()                                                 │
│ + send_request(svc, inst, method, payload): future<MessagePtr>              │
│ + send_request_no_return(...): bool                                         │
│ + subscribe_event(svc, inst, eg, event, version): bool                      │
│ + offer_service(config) / stop_offer_service(svc, inst)                     │
│ + register_method_handler(method, handler)                                  │
│ + send_response(request, payload) / send_error_response(request, code)      │
│ + notify_event(event, payload)                                              │
│ + handle_message(message)                                                   │
└────────────────────────────────────────────────────────────────────────────┘
         ▲                                                    ▲
         │ inherits                                           │ inherits
         │                                                    │
┌────────┴────────┐                                ┌─────────┴────────┐
│ ServiceSkeleton │                                │  ServiceProxy    │
├─────────────────┤                                ├──────────────────┤
│ - app_: shared_ptr<Application>                  │ - app_: shared_ptr<Application>
│ - service_id_: ServiceId                         │ - service_id_: ServiceId
│ - instance_id_: InstanceId                       │ - instance_id_: InstanceId
│ - method_handlers_: map<MethodId, Handler>       │ - available_: atomic<bool>
│ - field_getters_: map<FieldId, Getter>           │ - event_handlers_: map<EventId, Handler>
│ - field_setters_: map<FieldId, Setter>           │ - availability_handler_: Handler
│ - events_: map<EventId, EventConfig>             ├──────────────────┤
├─────────────────┤                                │ + call_method(): optional<PayloadData>
│ + init()        │                                │ + call_method_async(): future<optional>
│ + offer()       │                                │ + call_method_no_return(): bool
│ + stop_offer()  │                                │ + subscribe_event(): bool
│ + notify_event()│                                │ + get_field() / set_field()
│ + notify_field()│                                │ + is_available(): bool
├─────────────────┤                                │ + set_availability_handler()
│ # register_method()                              └──────────────────┘
│ # register_field_getter()
│ # register_field_setter()
│ # register_event()
│ # register_field()
└─────────────────┘
```

---

## 7. 线程模型

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           主线程 (Main Thread)                           │
│                    负责: 应用逻辑、回调处理                               │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         │                          │                          │
         ▼                          ▼                          ▼
┌─────────────────┐      ┌─────────────────┐      ┌─────────────────┐
│  UDP Receive    │      │  TCP Accept     │      │   SD Worker     │
│    Thread       │      │    Thread       │      │    Thread       │
│                 │      │                 │      │                 │
│ 接收UDP消息      │      │ 接受新连接       │      │ 处理SD消息      │
│ 组播消息处理     │      │                 │      │ TTL检查         │
│                 │      │                 │      │ 周期性Offer     │
└─────────────────┘      └─────────────────┘      └─────────────────┘
                                │
                                ▼
                       ┌─────────────────┐
                       │ TCP Connection  │
                       │ Receive Threads │
                       │ (每个连接一个)   │
                       └─────────────────┘
```

**设计要点**:
- UDP: 单线程接收，通过回调分发到主线程
- TCP: 独立 Accept 线程，每个连接独立接收线程
- Service Discovery: 独立工作线程处理定时任务

---

## 8. 状态机

### 8.1 服务状态机

```
                    ┌───────────┐
                    │   IDLE    │
                    └─────┬─────┘
                          │ offer()
                          ▼
                    ┌───────────┐
         ┌─────────│  OFFERING │◀────────┐
         │         └─────┬─────┘         │
         │               │               │
         │               ▼               │
         │         ┌───────────┐         │
         │    ┌───▶│ AVAILABLE │         │
         │    │    └─────┬─────┘         │
         │    │          │ stop_offer()  │
         │    └──────────┘               │
         │                               │
         └───────────────────────────────┘
```

### 8.2 订阅状态机

```
                    ┌───────────┐
                    │   IDLE    │
                    └─────┬─────┘
                          │ subscribe_event()
                          ▼
                    ┌───────────┐
                    │ SUBSCRIBING│
                    └─────┬─────┘
                          │
              ┌───────────┴───────────┐
              │                       │
              ▼                       ▼
        ┌───────────┐           ┌───────────┐
        │ SUBSCRIBED│           │   FAILED  │
        └─────┬─────┘           └───────────┘
              │ unsubscribe()
              ▼
        ┌───────────┐
        │  STOPPING │
        └───────────┘
```

---

## 9. 配置参数

### 9.1 Runtime 配置

| 参数           | 默认值           | 说明                     |
|---------------|-----------------|--------------------------|
| name          | ""              | 运行时名称               |
| address       | "127.0.0.1"     | 本地IP地址               |
| udp_port      | 0 (auto)        | UDP端口(0=自动分配)      |
| tcp_port      | 0 (auto)        | TCP端口(0=自动分配)      |
| enable_sd     | true            | 启用服务发现             |

### 9.2 Service Discovery 配置

| 参数                       | 默认值           | 说明                     |
|---------------------------|-----------------|--------------------------|
| multicast_address         | 224.244.224.245 | 组播地址                 |
| port                      | 30490           | SD端口                   |
| initial_delay_min         | 50ms            | 初始延迟最小值           |
| initial_delay_max         | 150ms           | 初始延迟最大值           |
| repetition_base_delay     | 30ms            | 重传基础延迟             |
| repetition_max            | 3               | 最大重传次数             |
| cyclic_offer_delay        | 2000ms          | 周期性Offer间隔          |
| offer_cancellation_delay  | 5000ms          | Offer取消延迟            |

---

## 10. 限制与改进方向

### 当前限制
1. **SOME/IP-TP**: 传输协议支持有限
2. **状态机**: 基础的服务发现状态机
3. **IPv6**: 未完全实现
4. **安全**: 无安全特性
5. **并发**: 单线程消息处理

### 改进建议
1. 添加完整的 SOME/IP-TP 支持用于大数据传输
2. 实现完整的状态机 (PRS-SOMEIPSD-00355)
3. 添加 TLS/DTLS 加密支持
4. 实现多线程消息处理
5. 添加性能监控和统计

---

## 11. 参考资料

- [AUTOSAR SOME/IP Specification](https://www.autosar.org/standards/foundation/)
- [vsomeip](https://github.com/COVESA/vsomeip) - 参考实现
- SOME/IP Protocol Specification v1.1.0

---

*文档版本: 1.0*
*生成日期: 2026-03-06*
