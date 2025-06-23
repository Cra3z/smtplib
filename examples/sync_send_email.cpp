#include <iostream>
#ifdef SMTPLIB_USE_BOOST_ASIO
#include <boost/asio/io_context.hpp>
#else
#include <asio/io_context.hpp>
#endif
#include <smtplib/smtplib.h>

int main(int argc, char** argv) try {
    if (argc != 4) {
        std::cout << "usage: " << argv[0] << " <from> <password> <to>\n";
        return EXIT_FAILURE;
    }

    smtplib::net::io_context io_context;
    smtplib::client cli{
        io_context.get_executor(),
        argv[1],
        argv[2],
        smtplib::security::ssl
    };

    cli.connect("smtp.qq.com", 465); // host and port
    cli.send({
        .from = argv[1],
        .to = argv[3],
        .subject = "Sync Send Test",
        .body = "Hello!"
    });

    return 0;
}
catch (const std::exception& ex) {
    std::cerr << "[ERROR] " << ex.what() << std::endl;
}