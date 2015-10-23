
#include "chat_structures.h"

#include <boost/fusion/include/adapt_struct.hpp>

#define BOOST_SPIRIT_USE_PHOENIX_V3

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>

#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>

namespace phoenix = boost::phoenix;
namespace proto   = boost::proto;
namespace qi      = boost::spirit::qi;
namespace karma   = boost::spirit::karma;
namespace ascii   = boost::spirit::ascii;


BOOST_FUSION_ADAPT_STRUCT (
    host_info,
    (std::string, address)
    (unsigned short, port)
)

BOOST_FUSION_ADAPT_STRUCT (
    connect_req,
    (std::string, from)
    (std::string, room)
    (host_info, host)
)

BOOST_FUSION_ADAPT_STRUCT (
    connect_res,
    (int, status)
    (std::string, host_id)
    (host_info_opt, host)
)

BOOST_FUSION_ADAPT_STRUCT (
    message,
    (std::string, from)
    (std::string, body)
)

template <template <typename> class Generator, typename T, typename Buffer>
inline bool encode(T const& msg, Buffer& buf)
{
    typedef std::back_insert_iterator<Buffer> iter_type;
    
    static const Generator<iter_type> g;
    
    iter_type it(buf);
    return karma::generate(it, g, msg);
}

template <typename Iterator>
struct connect_req_gen : karma::grammar<Iterator, connect_req()>
{
    using rule_str_t = karma::rule<Iterator, std::string()>;
    
    rule_str_t id_;
    karma::rule<Iterator, host_info()> host_;
    karma::rule<Iterator, connect_req()> pdu_;
    
    connect_req_gen() : connect_req_gen::base_type(pdu_),
        id_ (karma::repeat(1, 16) [karma::char_("0-9a-zA-Z@.")]),
        host_ (
            karma::lit("{\"address\":\"") << karma::string << "\","
            << "\"port\":" << karma::short_ << '}'
        ),
        pdu_ (
            karma::lit("connect-req:{")
            << karma::lit("\"from\":\"") << id_ << "\","
            << karma::lit("\"room\":\"") << id_ << "\","
            << karma::lit("\"host\":") << host_ << '}'
        )
    {}
};

template <typename Iterator>
struct connect_res_gen : karma::grammar<Iterator, connect_res()>
{
    karma::rule<Iterator, host_info_opt()> host_;
    karma::rule<Iterator, connect_res()> pdu_;
    
    connect_res_gen() : connect_res_gen::base_type(pdu_),
        host_ (
            -(karma::lit(",\"host\":") 
                << karma::lit("{\"address\":\"") << karma::string << "\","
                << "\"port\":" << karma::short_ << '}')
        ),
        pdu_ (
            karma::lit("connect-res:{")
            << karma::lit("\"status\":") << karma::int_ 
            << karma::string 
            << host_ 
            << '}'
        )
    {}
};

template <typename Iterator>
struct message_gen : karma::grammar<Iterator, message()>
{
    using rule_str_t = karma::rule<Iterator, std::string()>;
    
    rule_str_t id_, body_;
    karma::rule<Iterator, message()> pdu_;
    
    message_gen() : message_gen::base_type(pdu_),
        id_ (karma::repeat(1, 16) [karma::char_("0-9a-zA-Z@.")]),
        body_ (
            karma::lit("body:{") 
            << "len:" << karma::int_ [ karma::_1 = phoenix::bind<size_t>(&std::string::length, karma::_val) ]  << ','
            << "msg:\"" << karma::string [ karma::_1 = karma::_val ] << "\"}"
        ),       
        pdu_ (
            karma::lit("message") << ':' << '{'
            << karma::lit("from:\"") << id_ << "\","
            //<< karma::lit("\"to\":\"") << id_ << "\","
            << body_ << '}'
        )
    {}
};

template <template <typename, typename> class Grammar, typename Buffer, typename T>
inline bool decode(Buffer const& buf, T& pdu)
{
    typedef typename Buffer::const_iterator iter_type;
    
    static const Grammar<iter_type, ascii::space_type> g;
    
    auto b = std::begin(buf);
    auto e = std::end(buf);
    
    return qi::phrase_parse(b, e, g, ascii::space, pdu);
}

template <typename Iterator, typename Skipper>
struct connect_req_gram : qi::grammar<Iterator, connect_req(), Skipper>
{
    qi::rule<Iterator, std::string()> id_;
    qi::rule<Iterator, std::string()> address_;
    qi::rule<Iterator, host_info()> host_;
    qi::rule<Iterator, connect_req(), Skipper> pdu_;
    
    connect_req_gram() : connect_req_gram::base_type(pdu_),
        id_ (qi::repeat(1, 16) [qi::char_("0-9a-zA-Z@.")]),
        address_ (qi::repeat(4, 64) [qi::char_("0-9a-zA-Z@.")]),
        host_ (
            qi::char_('{') >> "\"address\"" >> ':' >> '"' >> address_ [phoenix::at_c<0>(qi::_val) = qi::_1] >> '"' >> ','
            >> qi::lit("\"port\"") >> ':' >> qi::short_ [ phoenix::at_c<1>(qi::_val) = qi::_1 ] >> '}'
        ),
        pdu_ (
            qi::lit("connect-req") >> ':' >> '{'
            >> qi::lit("\"from\"") >> ':' >> '"' >> id_ >> '"' >> ','
            >> qi::lit("\"room\"") >> ':' >> '"' >> id_ >> '"' >> ','
            >> qi::lit("\"host\"") >> ':' >> host_ >> '}'
        )   
    {}
};

template <typename Iterator, typename Skipper>
struct connect_res_gram : qi::grammar<Iterator, connect_res(), Skipper>
{
    qi::rule<Iterator, std::string()> id_;
    qi::rule<Iterator, std::string()> address_;
    qi::rule<Iterator, host_info(), Skipper> host_;
    qi::rule<Iterator, connect_res(), Skipper> pdu_;
    
    connect_res_gram() : connect_res_gram::base_type(pdu_),  
        id_ (qi::repeat(1, 16) [qi::char_("0-9a-zA-Z@.")]),
        address_ (qi::repeat(4, 64) [qi::char_("0-9a-zA-Z@.")]),
        host_ (
                qi::lit("\"host\"") >> ':' 
                >> '{' >> "\"address\"" >> ':' >> '"' >> address_ >> '"' >> ','
                >> "\"port\"" >> ':' >> qi::short_ >> '}'
        ),
        pdu_ (
            qi::lit("connect-res") >> ':' >> '{'
            >> "\"status\"" >> ':' >> qi::int_ 
            >> id_
            >> -(',' >> host_)
            >> '}'
        )
    {}
};

template <typename Iterator, typename Skipper>
struct message_gram : qi::grammar<Iterator, message(), Skipper>
{
    
    qi::rule<Iterator, std::string()> id_;
    qi::rule<Iterator, std::string(int)> msg_;
    qi::rule<Iterator, std::string(), qi::locals<int>> body_;
    qi::rule<Iterator, message(), Skipper> pdu_;
    
    message_gram() : message_gram::base_type(pdu_),
        id_ (qi::repeat(1, 16) [qi::char_("0-9a-zA-Z@.")]),
        msg_ (qi::repeat(qi::_r1) [qi::char_]),
        body_ (
            qi::lit("body") >> ':' >> '{'
            >> "len" >> ':' >> qi::short_ [ qi::_a = qi::_1 ] >> ','
            >> "msg" >> ':' >> '"' >> qi::lexeme [ msg_(qi::_a) [ qi::_val = qi::_1 ] ] >> '"' >> '}'
        ),
        pdu_ (
            qi::lit("message") >> ':' >> '{'
            >> qi::lit("from") >> ':' >> '"' >> id_ >> '"' >> ','
            //>> qi::lit("\"to\"") >> ':' >> '"' >> id_ >> '"' >> ','
            >> body_ >> '}'
        )
    {}
};
    
bool encode_connection_req(const connect_req& msg, buffer_t& buf)
{
    return encode<connect_req_gen>(msg, buf);
}

bool encode_connection_res(const connect_res& msg, buffer_t& buf)
{
    return encode<connect_res_gen>(msg, buf);
}

bool encode_message(const message& msg, buffer_t& buf)
{
    return encode<message_gen>(msg, buf);
}

bool decode_connect_req(const buffer_t& buf, connect_req& msg)
{
    return decode<connect_req_gram>(buf, msg);  
}

bool decode_connect_res(const buffer_t& buf, connect_res& msg)
{
    return decode<connect_res_gram>(buf, msg);
}

bool decode_message(const buffer_t& buf, message& msg)
{
    return decode<message_gram>(buf, msg);
}