
#include "chat_server.h"
#include "chat_structures.h"

#define PRINT_DEBUG(...) printf(__VA_ARGS__)

void server_session::do_read()
{
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(buf_.data(), buf_.capacity()),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
            if (ec)
            {
                if (!id_.empty())
                {
                    rooms_.erase(id_);
                }
                
                socket_.close();
                return;
            }
            
            auto r = buf_.consume(length);
            
            if (r == buffer_t::intermediate)
            {
                do_read();
            }
            else if (r == buffer_t::ok)
            {
                if (state_ == accept_request)
                {
                    connect_req req;
                    if (decode_connect_req(buf_, req))
                    {
                        connect_res res {0};
                        auto found = rooms_.find(req.room) != rooms_.end();
                        auto& room = rooms_[req.room];
                        
                        if (found)
                        {
                            PRINT_DEBUG ("Room %s found\n", req.room.c_str());
                            res.host = room.host;
                            state_ = close_connection;
                        }
                        else
                        {
                            PRINT_DEBUG ("Room %s created\n", req.room.c_str());
                            state_ = accept_info;
                            
                            id_ = req.room;
                            room.host_id = req.from;
                            room.host = req.host;
                        }
                        
                        res.host_id = room.host_id;
                        
                        // send response
                        buf_.reset();
                        if (encode_connection_res(res, buf_))
                        {
                            do_write();
                        }
                    }
                    else
                    {
                        socket_.close();
                    }
                }
                else if (state_ == accept_info)
                {
                    // todo: handle history here
                }
            }
            else
            {
                socket_.close();
            }
        });
}   

void server_session::do_write()
{
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(buf_.data(), buf_.length()),
        [this, self](boost::system::error_code ec, std::size_t)
        {
            if (!ec)
            {
                if (state_ == close_connection)
                {
                    // Initiate graceful connection closure.
                    boost::system::error_code ignored_ec;
                    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                        ignored_ec);
                }
                else
                {
                    buf_.reset();
                    do_read();
                }
            }

            if (ec != boost::asio::error::operation_aborted)
            {
            }
        });
}
