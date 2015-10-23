

#include "chat_server.h"

void chat_server::do_accept()
{
    acceptor_.async_accept(socket_,
        [this](boost::system::error_code ec)
        {
            if (!ec)
            {
                std::cout << "new user accepted" << std::endl;
                std::make_shared<server_session>(std::move(socket_), rooms_)->start();
            }

            do_accept();
        });
}

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Usage: chat_server <port> [<port> ...]\n";
            return 1;
        }

        boost::asio::io_service io;

//         std::list<chat_server> servers;
//         for (int i = 1; i < argc; ++i)
//         {
//         tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[i]));
//         servers.emplace_back(io_service, endpoint);
//         }
        
        tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[1]));
        
        chat_server server(io, endpoint);

        io.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    
    return 0;
}
