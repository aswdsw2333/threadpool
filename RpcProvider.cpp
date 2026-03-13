#include "RpcProvider.h"
#include "src/rpc.pb.h" // 确保包含你的 RpcHeader
#include <iostream>




void RpcProvider::Run(int port, int thread_nums) {
    // 初始化底层的 Epoll Reactor
    Tcpserver_epoll server(port);
    server.setThreadNum(thread_nums);

    // 【最致命的一句！】必须把 RpcProvider 自己的 onMessage 绑定进去！
    // 必须用 std::bind，因为 onMessage 现在是类的成员函数！
    server.setMessageCallback(
        std::bind(&RpcProvider::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );

    // 启动死循环
    server.start();
}



// =========================================================
// 1. 注册服务 (神仙级别的反射运用：自动提取服务名和方法名)
// =========================================================
void RpcProvider::NotifyService(google::protobuf::Service* service) {
    ServiceInfo service_info;
    service_info.m_service = service; // 记录服务对象

    // 获取服务对象的描述符 (说明书)
    const google::protobuf::ServiceDescriptor* pserviceDesc = service->GetDescriptor();

    // 提取服务名 (例如 "UserService")
    std::string service_name = pserviceDesc->name();

    // 获取该服务下有几个方法
    int method_cnt = pserviceDesc->method_count();

    std::cout << "=> 准备发布服务: " << service_name << std::endl;

    // 遍历所有方法，把它们的名字和描述符存入内层字典
    for (int i = 0; i < method_cnt; ++i) {
        const google::protobuf::MethodDescriptor* pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();

        // 插入内层字典
        service_info.m_methodMap[method_name] = pmethodDesc;
        std::cout << "  -- 注册方法: " << method_name << std::endl;
    }

    // 将整个服务信息插入外层路由表
    m_serviceMap[service_name] = service_info;
}

// =========================================================
// 2. 网络回调 (处理二进制数据，动态生成对象并分发)
// =========================================================
void RpcProvider::onMessage(const TcpConnectionPtr& conn, Buffer* buf, long long time) {
    // 解决粘包/半包的核心循环
    while (buf->readableBytes() >= 4) {

        // 1. 偷看前 4 个字节 (记录的是 RpcHeader 的序列化长度)
        int32_t header_size = buf->peekInt32();

        // 防御性编程
        if (header_size > 65536 || header_size < 0) {
            conn->shutdown();
            break;
        }

        // 2. 判断【信头】是否完全到达？
        // 4 字节的长度 + header_size
        if (buf->readableBytes() < 4 + header_size) {
            break; // 半包，退出等数据
        }

        // 3. 偷取【信头】字符串 (注意：只偷看，不准 retrieve 移动指针！因为正文可能还没到)
        std::string header_str(buf->peek() + 4, header_size);

        // 4. 反序列化 RpcHeader
        RpcMeta::RpcHeader rpcHeader;
        if (!rpcHeader.ParseFromString(header_str)) {
            std::cerr << "RpcHeader 解析失败！" << std::endl;
            conn->shutdown();
            break;
        }

        // 5. 从信头中提取关键信息
        std::string service_name = rpcHeader.service_name();
        std::string method_name = rpcHeader.method_name();
        uint32_t args_size = rpcHeader.args_size(); // 【关键】提取正文参数长度

        // 6. 判断【完整的包】(4 + 信头长度 + 正文长度) 是否全部到达！
        if (buf->readableBytes() < 4 + header_size + args_size) {
            break; // 正文参数还没到齐，分包了，退出等数据
        }

        // ================== 数据终于全齐了，可以放心地移动 Buffer 指针了！==================
        buf->retrieve(4);                // 移走 4 字节长度
        buf->retrieve(header_size);      // 移走信头
        std::string args_str = buf->retrieveAsString(args_size); // 移走并取出真正的参数

        // 打印调试信息
        std::cout << "============== 收到 RPC 请求 ==============" << std::endl;
        std::cout << "服务: " << service_name << " | 方法: " << method_name << std::endl;
        std::cout << "参数长度: " << args_size << " 字节" << std::endl;
        std::cout << "===========================================" << std::endl;

        // ================== 反射调用业务逻辑 (必须在 while 内) ==================
        auto it = m_serviceMap.find(service_name);
        if (it == m_serviceMap.end()) {
            std::cerr << "服务不存在: " << service_name << std::endl;
            continue; // 错误包，跳过，处理下一个包
        }
        ServiceInfo& service_info = it->second;

        auto mit = service_info.m_methodMap.find(method_name);
        if (mit == service_info.m_methodMap.end()) {
            std::cerr << "方法不存在: " << method_name << std::endl;
            continue;
        }

        google::protobuf::Service* service = service_info.m_service;
        const google::protobuf::MethodDescriptor* method = mit->second;

        // 动态生成 Request 和 Response 空壳
        google::protobuf::Message* request = service->GetRequestPrototype(method).New();
        google::protobuf::Message* response = service->GetResponsePrototype(method).New();

        // 把网络数据反序列化到 request 里
        if (!request->ParseFromString(args_str)) {
            std::cerr << "Request 参数解析失败" << std::endl;
            continue;
        }

        // 终极奥义：绑定回调
        google::protobuf::Closure* done = google::protobuf::NewCallback<RpcProvider, const TcpConnectionPtr&, google::protobuf::Message*>(
            this, &RpcProvider::sendRpcResponse, conn, response
        );
        //NewCallback 是 Protobuf 提供的 “回调绑定工具”，作用是：
        //创建一个 Closure 子类对象（匿名）；
        //    把「回调函数、所属对象、参数」绑定到这个对象中；
        //    这个对象的 Run() 方法被调用时，会自动执行绑定的函数。
        // 当调用 done->Run() 时，就会执行 this->sendRpcResponse(conn, response)。
        //CallMethod 是 “方法调度器”—— 框架层传入 method 描述符后，它会自动匹配到对应的业务方法（如 Login），完成基类指针到具体业务类指针的转换，最终调用你手写的业务逻辑。
        // 呼叫业务代码！
        service->CallMethod(method, nullptr, request, response, done);
    }
}

// =========================================================
// 3. 通用发送回调 (业务执行完，打包发送回客户端)
// =========================================================

// 假设这些信息通过某种方式传递进来，或者你从 response 的 Descriptor 中获取
void RpcProvider::sendRpcResponse(const TcpConnectionPtr& conn, google::protobuf::Message* response)
{
    if (response == nullptr) return;

    // 1. 序列化业务数据 (修正变量名：使用参数 response)
    std::string response_data;
    if (!response->SerializeToString(&response_data)) {
        std::cerr << "序列化响应失败!" << std::endl;
        delete response;
        return;
    }

    // 2. 构造响应头 (RpcHeader)
    // 注意：响应包通常只需要告知数据长度。
    // 如果你的协议要求响应也带 service_name，可以从 response 描述符获取
    RpcMeta::RpcHeader header;
    header.set_service_name(response->GetDescriptor()->full_name()); // 示例：自动获取类型全名
    header.set_args_size(response_data.size());

    std::string header_str;
    if (!header.SerializeToString(&header_str)) {
        std::cerr << "序列化 Header 失败!" << std::endl;
        delete response;
        return;
    }

    // 3. 封包 (基于长度的二进制流布局)
    // 布局设计：[Total Len (4B)] + [Header Len (4B)] + [Header Data] + [Body Data]
    uint32_t header_size = static_cast<uint32_t>(header_str.size());

    Buffer sendBuf;

    // 写入 Header 长度 (4字节)
    sendBuf.appendInt32(header_size);
    // 写入 Header 二进制数据
    sendBuf.append(header_str);
    // 写入 业务响应 二进制数据
    sendBuf.append(response_data);

    // 4. 发送并清理
    // 发送整个缓冲区的数据
    conn->send(sendBuf.retrieveAllAsString());

    // RPC 调用完成后，Provider 负责释放由框架 new 出来的 response 对象
    delete response;
}
