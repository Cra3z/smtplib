#include <iostream>
#ifdef SMTPLIB_USE_BOOST_ASIO
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#else
#include <asio/co_spawn.hpp>
#include <asio/io_context.hpp>
#endif
#include <smtplib/smtplib.h>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cout << "usage: " << argv[0] << " <from> <password> <to>\n";
        return EXIT_FAILURE;
    }

    smtplib::net::io_context io_context;
    smtplib::client cli{
        io_context.get_executor(),
        argv[1],
        argv[2],
        smtplib::security::starttls
    };

    smtplib::net::co_spawn(
        io_context,
        [&]() -> smtplib::net::awaitable<void> {
            co_await cli.async_connect("smtp.qq.com", 587); // host and port
            co_await cli.async_send({
                .from = argv[1],
                .to = argv[3],
                .subject = "Async Send Test",
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

    return 0;
}