#include "Shared/macros.h"

#define SERVERDOMAIN "google.com" // coliru IP at the time
#define SERVERPORT "80"
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <iomanip>

namespace asio = boost::asio;
using boost::asio::ip::tcp;
using boost::system::error_code;
using namespace std::chrono_literals;

class con_handler : public boost::enable_shared_from_this<con_handler> {
public:
    con_handler(asio::io_service& io_service) :
        server_socket(io_service),
        client_socket(io_service)
    {
        boost::asio::ip::tcp::resolver resolver(io_service);
        boost::asio::ip::tcp::resolver::query query(SERVERDOMAIN, SERVERPORT);
        boost::asio::ip::tcp::resolver::iterator i = resolver.resolve(query);
        boost::asio::ip::tcp::endpoint end = i->endpoint();
        server_socket.connect(end);
    }
    // creating the pointer
    using pointer = boost::shared_ptr<con_handler>;
    static pointer create(asio::io_service& io_service) {
        return pointer(new con_handler(io_service));
    }

    //socket creation
    tcp::socket& socket() {
        return client_socket;
    }

    void start() {
        Log(("Got request from: " + c_to_s.from.remote_endpoint().address().to_string() + "\n").c_str());

        c_to_s.run_relay(shared_from_this());
        s_to_c.run_relay(shared_from_this());
    }

private:
    tcp::socket server_socket;
    tcp::socket client_socket;
    enum { max_length = 1024 };

    struct relay {
        tcp::socket& from, & to;
        std::array<char, max_length> buf{};

        void run_relay(pointer self) {
            from.async_read_some(asio::buffer(buf),
                [this, self](error_code ec, size_t n) {
                    if (ec) return handle(from, ec);

                    
                   //std::cout
                   // << "From " << from.remote_endpoint()
                   // << ": " << std::quoted(std::string_view(buf.data(), n))
                   // << std::endl;
                    
                    async_write(to, asio::buffer(buf, n), [this, self](error_code ec, size_t) {
                        if (ec) return handle(to, ec);
                        run_relay(self);
                        });
                });
        }

        void handle(tcp::socket& which, error_code ec = {}) {
            if (ec == asio::error::eof) {
                // soft "error" - allow write to complete
                //std::cout << "EOF on " << which.remote_endpoint() << std::endl;
                which.shutdown(tcp::socket::shutdown_receive, ec);
            }

            if (ec) {
                from.cancel();
                to.cancel();

                std::string reason = ec.message();
                auto fep = from.remote_endpoint(ec),
                    tep = to.remote_endpoint(ec);
                //std::cout << "Stopped relay " << fep << " -> " << tep << " due to " << reason << std::endl;
            }
        }
    } c_to_s{ client_socket, server_socket, {0} },
        s_to_c{ server_socket, client_socket, {0} };
};

class Server {
    asio::io_service io_service;
    tcp::acceptor acceptor_;

    void start_accept() {
        // socket
        auto connection = con_handler::create(io_service);

        // asynchronous accept operation and wait for a new connection.
        acceptor_.async_accept(
            connection->socket(),
            [connection, this](error_code ec) {
                if (!ec) connection->start();
                start_accept();
            });
    }

public:
    Server() : acceptor_(io_service, { {}, PROXYPORT }) {
        start_accept();
    }

    void run() {
        Log("Starting proxy server on port: " SERVERPORT "\n");
        io_service.run(); // .run();
    }
};

int main() {
    Server().run();
}
