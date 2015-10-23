
#ifndef CHAT_STRUCTURES_H
#define CHAT_STRUCTURES_H

#include <string>

#include <boost/optional.hpp>

#include "chat_buffer.h"

struct host_info
{
    std::string address;
    unsigned short port;
};

using host_info_opt = boost::optional<host_info>;

struct connect_req
{
    std::string from;
    std::string room;
    host_info host;
};

struct connect_res
{
    int status;
    std::string host_id;
    host_info_opt host;
};

struct message
{
    std::string from;
    std::string body;
};

//bool encode_message(const std::string& from, buffer_t& buf))
bool encode_connection_req(connect_req const& msg, buffer_t& buf);
bool encode_connection_res(connect_res const& msg, buffer_t& buf);
bool encode_message(message const& msg, buffer_t& buf);

bool decode_connect_req(buffer_t const& buf, connect_req& msg);
bool decode_connect_res(buffer_t const& buf, connect_res& msg);
bool decode_message(buffer_t const& buf, message& msg);

#endif  