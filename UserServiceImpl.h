#include "rpc.pb.h" // 请确保这里的路径和你的 proto 生成文件路径一致
#include <google/protobuf/service.h>  
#include <google/protobuf/descriptor.h>
#include <iostream>
#include <string>

// 继承 Protobuf 自动生成的基类
class UserServiceImpl : public RpcMeta::UserService {
public:
    // 重写 Login 方法
    void Login(::google::protobuf::RpcController* controller,
        const ::RpcMeta::LoginRequest* request,
        ::RpcMeta::LoginResponse* response,
        ::google::protobuf::Closure* done) override
    {
        // 1. 打印后台接收到的日志 (纯英文，避免 Linux/Windows 终端乱码)
        std::cout << "\n[Business Layer] Received Login Request!" << std::endl;
        std::cout << "--> Account: " << request->name() << " | Password: " << request->pwd() << std::endl;

        // 2. 纯业务逻辑判断
        if (request->name() == "SuperAdmin" && request->pwd() == "654321") {
            response->set_success(true);
            // 【关键排雷点】：这里绝对不能有任何中文字符！
            response->set_msg("Login Success! Called by RpcProvider!");
        }
        else {
            response->set_success(false);
            // 【关键排雷点】：纯英文！
            response->set_msg("Login Failed! Invalid account or password.");
        }

        // 3. 业务处理完毕，执行回调函数，触发底层网络发送 (出餐铃)
        if (done) {
            done->Run();
        }
    }
};

