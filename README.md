en | [zh-cn](README.zh-CN.md)
# smtplib

smtplib is a simple `SMTP` mail sending library based on Asio, supporting both synchronous and coroutine-based asynchronous interfaces, as well as `SSL/TLS` and `STARTTLS`.

# Dependencies

* Boost.Asio >= 1.87 or standalone Asio >= 1.28
* OpenSSL >= 0.9.8
* fmt >= 10.1 (optional, not required if `std::format` is available)

# Build & Install

```shell
git clone https://github.com/Cra3z/smtplib.git
cd smtplib
mkdir <your-build-dir>
cmake -S . -B <your-build-dir> -DSMTPLIB_EXAMPLES=ON
cmake --build <your-build-dir>
cmake --install <your-build-dir> --prefix <your-install-dir>
```
The following two variables determine whether Boost.Asio or standalone Asio is used in the examples, and whether `std::format` or `fmt::format` is used:  
* `SMTPLIB_USE_BOOST_ASIO`: Use Boost.Asio
* `SMTPLIB_USE_FMT_FORMAT`: Use fmt library for formatting

For example:  
```shell
cmake -S . -B <your-build-dir> -DSMTPLIB_EXAMPLES=ON -DSMTPLIB_USE_BOOST_ASIO=ON -DSMTPLIB_USE_FMT_FORMAT=OFF
```
This will use Boost.Asio and `std::format` for output formatting.

## Example

Synchronous send with SSL:
```cpp
smtplib::net::io_context io_context;
smtplib::client cli{
    io_context.get_executor(),
    "your email",
    "password",
    smtplib::security::ssl
};
cli.connect("smtp.example.com", 465);
cli.send({
    .from = "your email",
    .to = "target email",
    .subject = "Test",
    .body = "Hello!"
});
```

Asynchronous send (C++20 coroutine) with STARTTLS:
```cpp
#include <smtplib/smtplib.h>
smtplib::net::io_context io_context;
smtplib::client cli{
    io_context.get_executor(),
    "your email",
    "password",
    smtplib::security::starttls
};
smtplib::net::co_spawn(
    io_context,
    [&]() -> smtplib::net::awaitable<void> {
        co_await cli.async_connect("smtp.example.com", 587);
        co_await cli.async_send({
            .from = "your email",
            .to = "target email",
            .subject = "Test",
            .body = "Hello!"
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
