
#pragma once
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>
#include <unordered_map>
#include <string>
#include <functional>
#include "Buffer.h"          
#include "TcpConnection.h"
#include "Tcpserver_epoll.h"
// 假设你有这些底层网络库的定义 (请替换成你实际的类名)
class TcpConnection;
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
class Buffer;

// 框架专用的专门用于发布 RPC 服务的网络对象类
class RpcProvider {
public:
    // 1. 核心接口：发布（注册）一个 RPC 服务
    // 参数接收基类指针，利用多态，可以接收任何生成的 Service (比如 UserServiceImpl)
    void NotifyService(google::protobuf::Service* service);

    // 2. 启动 RPC 服务节点，开始提供服务
    void Run(int port, int thread_nums);




private:
    // 组合底层网络对象：将你的 Epoll/Reactor 服务器封装进来
    // TcpServer m_tcpserver; (这里假定你有一个封装好的 TcpServer 类)

    // 新的路由表设计 (二维结构更专业)
    // 内部结构：用来保存服务对象，以及该服务下所有的方法描述符
    struct ServiceInfo {
        google::protobuf::Service* m_service; // 你的 UserServiceImpl 对象
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> m_methodMap; // 方法字典
    };

    // 真正的路由表：Key = 服务名 (例如 "UserService")，Value = 服务信息
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;

    // 底层网络收到数据的回调函数 (你之前的 onMessage 魔法全部搬到这里)
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, long long time);

    // 统一发送响应的回调 (Closure 的终点)
    void sendRpcResponse(const TcpConnectionPtr& conn, google::protobuf::Message* response);
};




//第三层：协议解析与传输层（TCP 粘包边界控制）
//由于 TCP 是面向字节流的（Byte Stream Protocol），它没有“数据包”的概念，底层随时可能发生数据的拆分与合并（粘包 / 半包）。
//
//Buffer（用户态缓冲区）
//
//核心机制：一个动态扩容的环形或连续内存缓冲区，具有读写双指针（readerIndex, writerIndex）。由于我们采用的是非阻塞 I / O（Non - blocking I / O），recv 读到的数据长度是不可预知的，必须先将数据追加到用户态 Buffer 中。
//
//基于长度字段的帧解码（Length - Field Based Frame Decoder）
//
//架构意义：你在 onMessage 中实现的那个 while (buf->readableBytes() >= 4) 循环，就是网络编程中经典的包边界识别逻辑。只有当 Buffer 中的数据大于等于[4字节长度头] + [Header] + [Body] 时，才完整移出（retrieve）一帧数据流交由上层处理。这是保障 RPC 协议一致性的“防火墙”。
//
//🧠 第四层：微服务业务路由层（RPC 与 反射分发）
//数据一旦被安全剥离出 TCP 字节流，就进入了你系统的“大脑”：RpcProvider。
//
//RPC Header（协议控制块）
//
//核心机制：包含 Service_Name（服务名）、Method_Name（方法名）和 Args_Size（参数长度）。这是实现服务路由的元数据（Metadata）。
//
//Protobuf Reflection（动态反射机制）
//
//核心机制：C++ 本身缺乏完善的 RTTI（运行时类型识别）和反射能力。你巧妙地利用了 Google Protobuf 提供的 ServiceDescriptor 和 MethodDescriptor，构建了一个基于 std::unordered_map 的两级服务注册表。
//
//架构意义（原型模式 / Prototype Pattern）：
//当框架解析出字符串 "UserService" 和 "Login" 时，它可以动态查找到对应的 C++ 类实例。最绝妙的是这两行代码：
//
//C++
//Message * request = service->GetRequestPrototype(method).New();
//Message* response = service->GetResponsePrototype(method).New();
//这意味着底层网络框架** 完全不需要在编译期知道具体的业务结构体是什么** 。它通过“原型对象”在运行时凭空克隆出了正确的 `LoginRequest` 空壳，并将二进制数据精准反序列化进去。这实现了** 网络通信层与业务逻辑层的 100 % 绝对解耦 * *。
//Closure 回调（异步完成令牌）
//
//核心机制：done->Run() 利用多态封装了底层的网络发送逻辑（sendRpcResponse）。业务层完全感知不到 TCP socket 的存在，只知道“我处理完了，按一下出餐铃”。

