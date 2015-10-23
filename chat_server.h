
#include <string>
#include <map>
#include <queue>

#include <boost/asio.hpp>

#include "chat_structures.h"

using boost::asio::ip::tcp;

struct history_entry
{
    std::string from;
    std::string message;
};

struct chat_room
{
    std::string id;
    std::string host_id;
    host_info host;
    std::queue<history_entry> history_;
};

typedef std::map<std::string, chat_room> room_map;

class server_session :
    public std::enable_shared_from_this<server_session>
{
public:
    
    server_session(tcp::socket socket, room_map& rooms) :
        socket_(std::move(socket)),
        rooms_ (rooms)
    {}
    
    void start()
    {
        do_read();
    }
    
protected:
    void do_read();
    void do_write();
    
private:
    tcp::socket socket_;
    
    buffer_t buf_;
    room_map& rooms_;
    std::string id_;
    
    enum { accept_request, accept_info, close_connection } state_{accept_request};
};

struct chat_server
{
    chat_server(boost::asio::io_service& io_service,
        const tcp::endpoint& endpoint) : 
      acceptor_(io_service, endpoint),
      socket_(io_service)
    {
        do_accept();
    }
    
    void do_accept();
    
private:
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    
    room_map rooms_;
};