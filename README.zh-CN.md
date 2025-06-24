简体中文 | [en](README.md)
# smtplib

smtplib是一个基于Asio的简单`SMTP`邮件发送库，支持同步与协程异步接口，支持 `SSL/TLS` 及 `STARTTLS`.

# 构建与安装

```shell
git clone https://github.com/Cra3z/smtplib.git
cd smtplib
mkdir <your-build-dir>
cmake -S . -B <your-build-dir> -DSMTPLIB_EXAMPLES=ON
cmake --build <your-build-dir>
cmake --install <your-build-dir> --prefix <your-install-dir>
```
通过以下两个变量决定使用的asio是Boost.Asio还是独立Asio以及使用`std::format`还是`fmt::format`:  
* `SMTPLIB_USE_BOOST_ASIO`：使用Boost.Asio
* `SMTPLIB_USE_FMT_FORMAT`：使用fmt库格式化

例如:  
```shell
cmake -S . -B <your-build-dir> -DSMTPLIB_EXAMPLES=ON -DSMTPLIB_USE_BOOST_ASIO=ON -DSMTPLIB_USE_FMT_FORMAT=OFF
```
将使用Boost.Asio, 同时使用`std::format`.

smtplib是一个header-only库, 所以其实你直接将`smtplib.h`复制到你需要的地方也可使用.  
当然更为推荐的方式是将stmplib作为子项目嵌入到你的项目中, 或者先将它安装再通过CMake的`find_package`引入到项目中.

## 构建要求

* CMake >= 3.26
* C++17或更新的C++标准, 默认使用C++20构建

### 依赖库
* [Boost.Asio](https://github.com/boostorg/asio) >= 1.87 或 [独立Asio](https://github.com/chriskohlhoff/asio) >= 1.28
* [openssl](https://github.com/openssl/openssl) >= 0.9.8
* [fmt](https://github.com/fmtlib/fmt) >= 10.1 (可选, 如果可以使用`std::format`则可不依赖)

# 示例

同步发送并使用SSL：
```cpp
smtplib::net::io_context io_context;
smtplib::client cli{
    io_context.get_executor(),
    "你的邮箱",
    "密码",
    smtplib::security::ssl
};
cli.connect("smtp.example.com", 465);
cli.send({
    .from = "你的邮箱",
    .to = "目标邮箱",
    .subject = "测试",
    .body = "你好!"
});
```

异步发送(C++20 协程)并使用STATRTTLS：
```cpp
smtplib::net::io_context io_context;
smtplib::client cli{
    io_context.get_executor(),
    "你的邮箱",
    "密码",
    smtplib::security::starttls
};
smtplib::net::co_spawn(
    io_context,
    [&]() -> smtplib::net::awaitable<void> {
        co_await cli.async_connect("smtp.example.com", 587);
        co_await cli.async_send({
            .from = "你的邮箱",
            .to = "目标邮箱",
            .subject = "测试",
            .body = "你好！"
        });
    },
    [](std::exception_ptr exp) {
        try {
            if (exp) {
                std::rethrow_exception(exp);
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "[ERROR] " << ex.what() << std::endl;
        }
    }
);
io_context.run();
```
