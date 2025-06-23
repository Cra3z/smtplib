#pragma once
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#ifdef SMTPLIB_USE_FMT_FORMAT
#include <fmt/core.h>
#else
#ifndef __cpp_lib_format
#error "smtplib requires C++20 std::format or {fmt} library."
#endif
#include <format>
#endif

#ifdef SMTPLIB_USE_BOOST_ASIO
#if defined(__cpp_impl_coroutine) && defined(__cpp_lib_coroutine)
#include <boost/asio/awaitable.hpp>
#endif
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#else
#if defined(__cpp_impl_coroutine) && defined(__cpp_lib_coroutine)
#include <asio/awaitable.hpp>
#endif
#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/read_until.hpp>
#include <asio/ssl.hpp>
#include <asio/streambuf.hpp>
#include <asio/write.hpp>
#endif

namespace smtplib {

#ifdef SMTPLIB_USE_BOOST_ASIO
    namespace net = boost::asio;
    namespace system = boost::system;
#else
    namespace net = asio;
    namespace system = asio;
#endif

#ifdef SMTPLIB_USE_FMT_FORMAT
    using fmt::format;
#else
    using std::format;
#endif

    namespace error {
        enum smtp_errc : int {
            service_not_available = 421,
            mailbox_unavailable = 450,
            local_error = 451,
            insufficient_storage = 452,

            syntax_error = 500,
            argument_error = 501,
            command_not_implemented = 502,
            bad_command_sequence = 503,
            command_parameter_not_implemented = 504,

            mailbox_not_found = 550,
            user_not_local = 551,
            exceeded_storage_allocation = 552,
            mailbox_name_invalid = 553,
            transaction_failed = 554
        };

        enum misc_errc : int {
            broken_response = 1
        };

        namespace detail {
            // ReSharper disable once CppPolymorphicClassWithNonVirtualPublicDestructor
            struct smtp_errc_category_ : system::error_category {
                const char* name() const noexcept override {
                    return "smtplib.error.smtp_errc";
                }

                std::string message(int code) const override {
                    switch (smtp_errc(code)) {
                    case service_not_available: return "service not available, closing transmission channel";
                    case mailbox_unavailable: return "requested mail action not taken: mailbox unavailable";
                    case local_error: return "requested action aborted: local error in processing";
                    case insufficient_storage: return "requested action not taken: insufficient storage";

                    case syntax_error: return "syntax error, command unrecognized";
                    case argument_error: return "syntax error in parameters or arguments";
                    case command_not_implemented: return "command not implemented";
                    case bad_command_sequence: return "bad sequence of commands";
                    case command_parameter_not_implemented: return "command parameter not implemented";

                    case mailbox_not_found: return "requested action not taken: mailbox unavailable";
                    case user_not_local: return "user not local; please try a different path";
                    case exceeded_storage_allocation: return "requested mail action aborted: exceeded storage allocation";
                    case mailbox_name_invalid: return "requested action not taken: mailbox name not allowed";
                    case transaction_failed: return "transaction failed";
                    default: return (format)("unknown ({})", code);
                    }
                }
            };

            inline constexpr smtp_errc_category_ smtp_errc_category{};

            // ReSharper disable once CppPolymorphicClassWithNonVirtualPublicDestructor
            struct misc_errc_category_ : system::error_category {
                const char* name() const noexcept override {
                    return "smtplib.error.misc_errc";
                }

                std::string message(int code) const override {
                    switch (misc_errc(code)) {
                    case broken_response: return "invalid response from SMTP server";
                    default: return (format)("unknown ({})", code);
                    }
                }
            };

            inline constexpr misc_errc_category_ misc_errc_category{};
        }

        inline system::error_code make_error_code(smtp_errc code) noexcept {
            return system::error_code{int(code), detail::smtp_errc_category};
        }

        inline system::error_code make_error_code(misc_errc code) noexcept {
            return system::error_code{int(code), detail::misc_errc_category};
        }
    }

    struct email {
        std::string from;
        std::string to;
        std::string subject;
        std::string body;
    };

    enum class security {
        none,
        ssl,
        starttls
    };

    using tls_method = net::ssl::context::method;

    namespace detail {
        template<typename TcpSocket>
        struct ssl_provider {
            explicit ssl_provider(
                TcpSocket socket,
                tls_method method
            ) : ssl_context(method),
                ssl_stream(std::move(socket), ssl_context) {}

            net::ssl::context ssl_context;
            net::ssl::stream<TcpSocket> ssl_stream;
        };
    }
}

template<>
struct smtplib::system::is_error_code_enum<smtplib::error::smtp_errc> : std::true_type {};

template<>
struct smtplib::system::is_error_code_enum<smtplib::error::misc_errc> : std::true_type {};

namespace smtplib {

    template<typename Executor>
    class basic_client {
    public:
        using executor_type = Executor;
        using tcp_socket = net::basic_stream_socket<net::ip::tcp, executor_type>;

#if defined(__cpp_impl_coroutine) && defined(__cpp_lib_coroutine)
        template<typename T>
        using async = net::awaitable<T, executor_type>;
#endif

    public:
        basic_client(
            executor_type executor,
            std::string_view username,
            std::string_view password,
            security s
        ) : executor_(executor),
            security_(s),
            username_(username),
            password_(password),
            sock_(executor) {}

        basic_client(
            executor_type executor,
            std::string_view username,
            std::string_view password,
            std::string_view host,
            std::uint_least16_t port,
            security s
        ) : basic_client(std::move(executor), username, password, s) {
            connect(host, port);
        }

        basic_client(const basic_client&) = delete;

        basic_client& operator= (const basic_client&) = delete;

        void connect(std::string_view host, std::uint_least16_t port) {
            net::ip::basic_resolver<net::ip::tcp, executor_type> resolver{executor_};
            auto endpoints = resolver.resolve(host, (format)("{}", port));

            host_ = host;

            if (security_ == security::ssl) {
                attach_ssl_(tls_method::sslv23_client);
                net::connect(ssl_provider_->ssl_stream.next_layer(), endpoints);
                ssl_provider_->ssl_stream.handshake(net::ssl::stream_base::client);
            }
            else {
                net::connect(sock_, endpoints);
            }
            read_response_();
        }

        void connect(net::ip::tcp::endpoint endpoint) {
            host_ = endpoint.address().to_string();

            if (security_ == security::ssl) {
                attach_ssl_(tls_method::sslv23_client);
                net::connect(ssl_provider_->ssl_stream.next_layer(), endpoint);
                ssl_provider_->ssl_stream.handshake(net::ssl::stream_base::client);
            }
            else {
                net::connect(sock_, endpoint);
            }
            read_response_();
        }

        void send(const email& email) {
            ehlo();
            if (security_ == security::starttls) start_tls_();
            auth_login_();
            mail_from_(email.from);
            rcpt_to_(email.to);
            data_(email.subject, email.from, email.to, email.body);
            quit_();
            if (security_ == security::starttls) detach_ssl_();
        }

#if defined(__cpp_impl_coroutine) && defined(__cpp_lib_coroutine)

        async<void> async_connect(std::string_view host, std::uint_least16_t port) {
            net::ip::basic_resolver<net::ip::tcp, executor_type> resolver{executor_};
            auto endpoints = co_await resolver.async_resolve(host, std::to_string(port));

            host_ = host;

            if (security_ == security::ssl) {
                attach_ssl_(tls_method::sslv23_client);
                co_await net::async_connect(ssl_provider_->ssl_stream.next_layer(), endpoints);
                co_await ssl_provider_->ssl_stream.async_handshake(net::ssl::stream_base::client);
            }
            else {
                co_await net::async_connect(sock_, endpoints);
            }
            co_await async_read_response_();
        }

        async<void> async_connect(net::ip::tcp::endpoint endpoint) {
            host_ = endpoint.address().to_string();

            if (security_ == security::ssl) {
                attach_ssl_(tls_method::sslv23_client);
                co_await net::async_connect(ssl_provider_->ssl_stream.next_layer(), endpoint);
                co_await ssl_provider_->ssl_stream.async_handshake(net::ssl::stream_base::client);
            }
            else {
                co_await net::async_connect(sock_, endpoint);
            }
            co_await async_read_response_();
        }


        async<void> async_send(email email) {
            co_await async_ehlo();
            if (security_ == security::starttls) co_await async_start_tls_();
            co_await async_auth_login_();
            co_await async_mail_from_(email.from);
            co_await async_rcpt_to_(email.to);
            co_await async_data_(email.subject, email.from, email.to, email.body);
            co_await async_quit_();
            if (security_ == security::starttls) detach_ssl_();
        }

#endif

    private:
        static std::string base64_encode_(std::string_view in) {
            static constexpr char table[]{"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
            std::string out;
            int val = 0, valb = -6;
            for (auto byte : in) {
                val = (val << 8) + byte;
                valb += 8;
                while (valb >= 0) {
                    out.push_back(table[(val >> valb) & 0x3f]);
                    valb -= 6;
                }
            }
            if (valb > -6) out.push_back(table[((val << 8) >> (valb + 8)) & 0x3f]);
            while (out.size() % 4) out.push_back('=');
            return out;
        }


        void attach_ssl_(net::ssl::context::method method) {
            if (ssl_provider_) {
                ssl_provider_->ssl_stream.next_layer() = std::move(sock_);
            }
            else {
                ssl_provider_.emplace(std::move(sock_), method);
            }
        }

        void detach_ssl_() {
            if (ssl_provider_ && ssl_provider_->ssl_stream.next_layer().is_open()) {
                sock_ = std::move(ssl_provider_->ssl_stream.next_layer());
            }
        }

        void ehlo() {
            send_command_((format)("EHLO {}\r\n", host_));
        }

        void start_tls_() {
            send_command_("STARTTLS\r\n");
            attach_ssl_(tls_method::tls_client);
            ssl_provider_->ssl_stream.handshake(net::ssl::stream_base::client);
            return ehlo();
        }

        void auth_login_() {
            send_command_("AUTH LOGIN\r\n");
            send_command_(base64_encode_(username_) + "\r\n");
            send_command_(base64_encode_(password_) + "\r\n");
        }

        void mail_from_(std::string_view from) {
            send_command_((format)("MAIL FROM:<{}>\r\n", from));
        }

        void rcpt_to_(std::string_view to) {
            send_command_((format)("RCPT TO:<{}>\r\n", to));
        }

        void data_(
            std::string_view subject,
            std::string_view from,
            std::string_view to,
            std::string_view body
        ) {
            std::string msg = (format)(
                "DATA\r\n"
                "Subject: {}\r\n"
                "From: {}\r\n"
                "To: {}\r\n"
                "\r\n{}\r\n.\r\n",
                subject,
                from,
                to,
                body
            );
            send_command_(msg);
        }

        void quit_() {
            send_command_("QUIT\r\n");
        }

        void send_command_(const std::string& cmd) {
            if (sock_.is_open()) {
                net::write(sock_, net::buffer(cmd));
            }
            else {
                net::write(ssl_provider_->ssl_stream, net::buffer(cmd));
            }
            read_response_();
        }

        void read_response_() {
            std::string line;
            net::streambuf buf;
            do {
                if (sock_.is_open()) {
                    net::read_until(sock_, buf, "\r\n");
                }
                else {
                    net::read_until(ssl_provider_->ssl_stream, buf, "\r\n");
                }
                std::istream is(&buf);
                std::getline(is, line);
                line.pop_back(); // remove '\r'

                if (line.size() < 4) {
                    throw system::system_error{error::misc_errc::broken_response};
                }

                if (line[0] != '2' && line[0] != '3') {
                    int code;
                    auto [_, ec] = std::from_chars(line.data(), line.data() + 3, code);
                    if (ec != std::errc{}) throw system::system_error{std::make_error_code(ec)};
                    throw system::system_error{error::smtp_errc(code)};
                }
            }
            while (line[3] == '-');
        }

#if defined(__cpp_impl_coroutine) && defined(__cpp_lib_coroutine)

        async<void> async_ehlo() {
            co_await async_send_command_((format)("EHLO {}\r\n", host_));
        }

        async<void> async_start_tls_() {
            co_await async_send_command_("STARTTLS\r\n");
            attach_ssl_(tls_method::tls_client);
            co_await ssl_provider_->ssl_stream.async_handshake(net::ssl::stream_base::client);
            co_await async_ehlo();
        }

        async<void> async_auth_login_() {
            co_await async_send_command_("AUTH LOGIN\r\n");
            co_await async_send_command_(base64_encode_(username_) + "\r\n");
            co_await async_send_command_(base64_encode_(password_) + "\r\n");
        }

        async<void> async_mail_from_(std::string_view from) {
            co_await async_send_command_((format)("MAIL FROM:<{}>\r\n", from));
        }

        async<void> async_rcpt_to_(std::string_view to) {
            co_await async_send_command_((format)("RCPT TO:<{}>\r\n", to));
        }

        async<void> async_data_(
            std::string_view subject,
            std::string_view from,
            std::string_view to,
            std::string_view body
        ) {
            std::string msg = (format)(
                "DATA\r\n"
                "Subject: {}\r\n"
                "From: {}\r\n"
                "To: {}\r\n"
                "\r\n{}\r\n.\r\n",
                subject,
                from,
                to,
                body
            );
            co_await async_send_command_(msg);
        }

        async<void> async_quit_() {
            co_await async_send_command_("QUIT\r\n");
        }

        async<void> async_send_command_(const std::string& cmd) {
            if (sock_.is_open()) {
                co_await net::async_write(sock_, net::buffer(cmd));
            }
            else {
                co_await net::async_write(ssl_provider_->ssl_stream, net::buffer(cmd));
            }
            co_await async_read_response_();
        }

        async<void> async_read_response_() {
            std::string line;
            net::streambuf buf;
            do {
                if (sock_.is_open()) {
                    co_await net::async_read_until(sock_, buf, "\r\n");
                }
                else {
                    co_await net::async_read_until(ssl_provider_->ssl_stream, buf, "\r\n");
                }
                std::istream is(&buf);
                std::getline(is, line);
                line.pop_back(); // remove '\r'

                if (line.size() < 4) {
                    throw system::system_error{error::misc_errc::broken_response};
                }

                if (line[0] != '2' && line[0] != '3') {
                    int code;
                    auto [_, ec] = std::from_chars(line.data(), line.data() + 3, code);
                    if (ec != std::errc{}) throw system::system_error{std::make_error_code(ec)};
                    throw system::system_error{error::smtp_errc(code)};
                }
            }
            while (line[3] == '-');
        }

#endif

    private:
        executor_type executor_;
        security security_;
        std::string host_;
        std::string username_;
        std::string password_;
        tcp_socket sock_;
        std::optional<detail::ssl_provider<tcp_socket>> ssl_provider_;
    };

    using client = basic_client<net::any_io_executor>;
}