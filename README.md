en | [zh-cn](README.zh-CN.md)
# smtplib

smtplib is a simple SMTP mail sending library based on Asio, supporting both synchronous and coroutine-based asynchronous interfaces, and supports `SSL/TLS` and `STARTTLS`.

# Build & Install

```shell
git clone https://github.com/Cra3z/smtplib.git
cd smtplib
mkdir <your-build-dir>
cmake -S . -B <your-build-dir> -DSMTPLIB_EXAMPLES=ON
cmake --build <your-build-dir>
cmake --install <your-build-dir> --prefix <your-install-dir>
```
The following two variables determine whether to use Boost.Asio or standalone Asio, and whether to use `std::format` or `fmt::format`:  
* `SMTPLIB_USE_BOOST_ASIO`: Use Boost.Asio
* `SMTPLIB_USE_FMT_FORMAT`: Use the fmt library for formatting

For example:  
```shell
cmake -S . -B <your-build-dir> -DSMTPLIB_EXAMPLES=ON -DSMTPLIB_USE_BOOST_ASIO=ON -DSMTPLIB_USE_FMT_FORMAT=OFF
```
This will use Boost.Asio and `std::format`.

smtplib is a header-only library, so you can simply copy `smtplib.h` to where you need it.  
However, it is recommended to embed smtplib as a subproject in your project, or install it first and then import it via CMake's `find_package`.

## Build Requirements

* CMake >= 3.26
* C++17 or newer (C++20 is used by default)

### Dependencies
* [Boost.Asio](https://github.com/boostorg/asio) >= 1.87 or [standalone Asio](https://github.com/chriskohlhoff/asio) >= 1.28
* [openssl](https://github.com/openssl/openssl) >= 0.9.8
* [fmt](https://github.com/fmtlib/fmt) >= 10.1 (optional, not needed if `std::format` is available)

# Examples

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
