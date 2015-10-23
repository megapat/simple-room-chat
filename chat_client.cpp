//
// chat_client.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <chrono>
#include <set>

#include <boost/asio.hpp>

#include "chat_structures.h"

#define PRINT_DEBUG(...) //printf(__VA_ARGS__)

using boost::asio::ip::tcp;

const unsigned short Port = 12345;

//----------------------------------------------------------------------
typedef buffer_t chat_message;
typedef std::deque<chat_message> chat_message_queue;

//----------------------------------------------------------------------

class chat_participant
{
public:
  virtual ~chat_participant() {}
  virtual void deliver(const chat_message& msg) = 0;
  
  std::string id;
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room
{
public:
  void join(chat_participant_ptr participant)
  {
    participants_.insert(participant);
    for (auto msg: recent_msgs_)
      participant->deliver(msg);
  }

  void leave(chat_participant_ptr participant)
  {
    participants_.erase(participant);
  }

  void deliver(const chat_message& msg, const std::string& from)
  {
    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > max_recent_msgs)
      recent_msgs_.pop_front();

    for (auto participant: participants_)
        if (participant->id != from)
            participant->deliver(msg);
  }

private:
  std::set<chat_participant_ptr> participants_;
  enum { max_recent_msgs = 100 };
  chat_message_queue recent_msgs_;
};

//----------------------------------------------------------------------

class chat_session
  : public chat_participant,
    public std::enable_shared_from_this<chat_session>
{
public:
  chat_session(tcp::socket socket, const std::string& id, chat_room& room) : 
      socket_(std::move(socket)),
      room_(room)
  {
  }

  void start()
  {
    room_.join(shared_from_this());
    //do_read_header();
    do_read();
  }

  void deliver(const chat_message& msg)
  {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
      do_write();
    }
  }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(read_msg_.data(), read_msg_.capacity()),
            [this, self](boost::system::error_code ec, std::size_t length)
            {
                if (ec)
                {
                    socket_.close();
                    return;
                }
                
                auto r = read_msg_.consume(length);  
                
                if (r == buffer_t::intermediate)
                {
                    do_read();
                }
                else if (r == buffer_t::ok)
                {
                    message msg;
                    
                    if (decode_message(read_msg_, msg))
                    {
                        if (id.empty()) id = msg.from;
                        room_.deliver(read_msg_, msg.from);
            
                        std::cout << id << "> " << msg.body << std::endl;
                    }
                     
                    do_read();
                }
                else
                {
                    room_.leave(shared_from_this());
                    socket_.close();
                }
            });
    }
        
    
//   void do_read_header()
//   {
//     auto self(shared_from_this());
//     boost::asio::async_read(socket_,
//         boost::asio::buffer(read_msg_.data(), chat_message::header_length),
//         [this, self](boost::system::error_code ec, std::size_t /*length*/)
//         {
//           if (!ec && read_msg_.decode_header())
//           {
//             do_read_body();
//           }
//           else
//           {
//             room_.leave(shared_from_this());
//           }
//         });
//   }
// 
//   void do_read_body()
//   {
//     auto self(shared_from_this());
//     boost::asio::async_read(socket_,
//         boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
//         [this, self](boost::system::error_code ec, std::size_t /*length*/)
//         {
//           if (!ec)
//           {
//             room_.deliver(read_msg_, id);
//             
//             std::cout.write(read_msg_.body(), read_msg_.body_length());
//             std::cout << "\n";
//             
//             do_read_header();
//           }
//           else
//           {
//             room_.leave(shared_from_this());
//           }
//         });
//   }

  void do_write()
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_,
        boost::asio::buffer(write_msgs_.front().data(),
          write_msgs_.front().length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
            {
              do_write();
            }
          }
          else
          {
            room_.leave(shared_from_this());
          }
        });
  }

  tcp::socket socket_;
  chat_room& room_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;
};

struct chat_server
{
    chat_server(boost::asio::io_service& io, unsigned short port) : 
      acceptor_ (io),
      socket_   (io),
      resolver_ (io),
      port_ (port)
    {}
    
    void start_accept(const std::string& id)
    {
        PRINT_DEBUG ("Start accepting in %s room\n", id.c_str());
        
        tcp::endpoint local(tcp::endpoint(tcp::v4(), port_));
        
        acceptor_.open(local.protocol());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        acceptor_.bind(local);
        acceptor_.listen();
        
        do_accept(id);
    }
    
    void do_accept(const std::string& id)
    {
        acceptor_.async_accept(socket_,
        [this, id](boost::system::error_code ec)
        {
          if (!ec)
          {
            PRINT_DEBUG ("New user accepted\n");
            std::make_shared<chat_session>(std::move(socket_), id, room_)->start();
          }

          do_accept(id);
        });
    }
    
protected:
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    tcp::resolver resolver_;
    
    unsigned short port_;
    chat_room room_;
};

typedef std::deque<chat_message> chat_message_queue;

class chat_client : public chat_server
{
public:
  chat_client(boost::asio::io_service& io_service, tcp::resolver::iterator it,
    const std::string room,
    const std::string id,
    unsigned short port = Port
  )
    : chat_server (io_service, port),
      io_service_ (io_service),
      socket_     (io_service),
      write_msgs_ (),
      
      remote_(it),
      srvsocket_  (io_service),
      room_ (room),
      id_   (id),
      port_ (port)
  {
    do_connect_server(remote_);
  }

  void write(const chat_message& msg)
  {
    io_service_.post(
        [this, msg]()
        {
          bool write_in_progress = !write_msgs_.empty();
          write_msgs_.push_back(msg);
          if (!write_in_progress)
          {
            do_write();
          }
        });
  }

  void close()
  {
    io_service_.post([this]() { socket_.close(); });
  }

private:
  void do_connect(tcp::resolver::iterator remote)
  {
      PRINT_DEBUG ("Connecting to %s\n", remote->host_name().c_str());
    boost::asio::async_connect(socket_, remote,
        [this](boost::system::error_code ec, tcp::resolver::iterator)
        {
          if (!ec)
          {
            std::cout << "system> You are connected. " << host_id_ << " is host of the room" << std::endl;
            //do_read_header();
            do_read();
          }
        });
  }
  
   void do_resolve(const std::string address, unsigned int port)
    {
        PRINT_DEBUG ("Resolving %s:%d ...\n", address.c_str(), port);
        tcp::resolver::query query(address, std::to_string(port));
        resolver_.async_resolve(query,
            [this](const boost::system::error_code& ec, tcp::resolver::iterator it)
            {
                if (!ec)
                {
                    do_connect(it);
                }
                else
                {
                    PRINT_DEBUG ("Failed to resolve host address! %s!\n", ec.message().c_str());
                }
            });
    }
    
    void do_read()
    {
        socket_.async_read_some(boost::asio::buffer(read_msg_.data(), read_msg_.capacity()),
            [this](boost::system::error_code ec, std::size_t length)
            {
                if (ec)
                {
                    socket_.close();
                    
                    std::cout << "system> " <<host_id_ << " left." << std::endl;
                    do_connect_server(remote_);
                    
                    return;
                }
                
                auto r = read_msg_.consume(length);  
                
                if (r == buffer_t::intermediate)
                {
                    do_read();
                }
                else if (r == buffer_t::ok)
                {
                    message msg;
                    
                    if (decode_message(read_msg_, msg))
                    {
                        std::cout << msg.from << "> " << msg.body << std::endl;
                    }
                     
                    do_read();
                }
                else
                {
                    socket_.close();
                }
            });
    }
    
  void do_write()
  {
      if (is_host_)
      {
          chat_server::room_.deliver(write_msgs_.front(), id_);
          write_msgs_.pop_front();
      }
      else
      {
        boost::asio::async_write(socket_,
            boost::asio::buffer(write_msgs_.front().data(),
            write_msgs_.front().length()),
            [this](boost::system::error_code ec, std::size_t /*length*/)
            {
            if (!ec)
            {
                write_msgs_.pop_front();
                if (!write_msgs_.empty())
                {
                do_write();
                }
            }
            else
            {
                std::cout << "do_write err: " << ec.message() << std::endl;
                socket_.close();
            }
            });
      }
  }
  
    void do_connect_server(tcp::resolver::iterator it)
    {
        boost::asio::async_connect(srvsocket_, it,
        [this](boost::system::error_code ec, tcp::resolver::iterator)
        {
          if (!ec)
          {
            do_send_request();
          }
          else
          {
            srvsocket_.close();
          }
        });
    }
 
   
    void do_send_request()
    {
        buf_.reset();
        
        connect_req req{id_, room_, {srvsocket_.local_endpoint().address().to_string(), port_}};
        
        if (!encode_connection_req(req, buf_))
        {
            std::cout << buf_.data()[buf_.length() - 1] << std::endl;
            
            srvsocket_.close();
            return;
        }
        
        boost::asio::async_write(srvsocket_, boost::asio::buffer(buf_.data(), buf_.length()),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                do_read_response();
            }
            else
            {
                srvsocket_.close();
            }
        });
    }
    
    void do_send_info()
    {
    }
    
    void do_read_response()
    {
        srvsocket_.async_read_some(boost::asio::buffer(buf_.data(), buf_.capacity()),
        [this](boost::system::error_code ec, std::size_t length)
        {
            if (ec)
            {
                srvsocket_.close();
                return;
            }
            
            auto r = buf_.consume(length);
            if (r == buffer_t::intermediate)
            {
                do_read_response();
            }
            else if (r == buffer_t::ok)
            {
                connect_res res;
                
                if (!decode_connect_res(buf_, res) or res.status != 0)
                {
                    srvsocket_.close();
                    return;
                }
                
                if (res.host)
                {
                    srvsocket_.close();
                    host_id_ = res.host_id;
                    is_host_ = false;
                    do_resolve(res.host.get().address, res.host.get().port);
                }
                else
                {
                    std::cout << "system> You are now host of the room." << std::endl;
                    
                    // need to become host
                    is_host_ = true;
                    start_accept(id_);
                }
            }
            else
            {
                srvsocket_.close();
            }
        });
    }

private:
  boost::asio::io_service& io_service_;
  tcp::socket socket_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;
  
    tcp::resolver::iterator remote_;
    tcp::socket srvsocket_;
    std::string room_;
    std::string id_;
    unsigned short port_;
    
    buffer_t buf_;
    
    std::string host_id_;
    bool is_host_{false};
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 6)
    {
      std::cerr << "Usage: chat_client <host> <port> <room> <name> <listen_port>\n";
      return 1;
    }
    
    boost::asio::io_service io_service;

    tcp::resolver resolver(io_service);
    auto remote = resolver.resolve({ argv[1], argv[2] });
    
    std::string room(argv[3]);
    std::string id(argv[4]);
    chat_client c(io_service, remote, room, id, std::atoi(argv[5]));

    std::thread t([&io_service](){ io_service.run(); });
    
    // give a chance thread to run
    std::this_thread::sleep_for(std::chrono::seconds(2)); 
    
    message input { id };
    while (std::getline(std::cin, input.body))
    {
        if (input.body.empty()) continue;
        
        std::cout << "\e[A" << "You> " << input.body << std::endl;
        
        chat_message msg;
        
        if (encode_message(input, msg))
        {
            c.write(msg);
        }
    }

    c.close();
    t.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
