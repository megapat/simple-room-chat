
#ifndef CHAT_BUFFER_H
#define CHAT_BUFFER_H

#include <cstring>
#include <string>
#include <iterator>

template <int N, char C = '\n'>
struct buffer
{
    static constexpr int max_size = N - 1;
    
    buffer() 
    {
        reset();
    }
    
    typedef char        value_type;
    typedef char*       iterator;
    typedef char const* const_iterator;
    
    iterator begin () { return data_; }
    iterator end   () { return data_ + size_; }

    const_iterator begin () const  { return data_; }
    const_iterator end   () const  { return data_ + size_; }
    
    char* data() { return data_; }
    int capacity() const { return N; }
    
    int length() const { return size_ + 1; }
    std::string str() const { return std::string(begin(), end()); }
    
    void reset() 
    { 
        size_ = 0; 
        std::memset(data_, C, N );
    }
    
    void push_back(char c)
    {
        if (size_ < max_size)
        {
            data_[size_++] = c;
        }
    }
    
    template <typename T>
    buffer& operator=(T const& rhs)
    {
        auto b = std::begin(rhs);
        auto e = std::end(rhs);
        auto d = std::distance(b, e);
        
        if (d <= N)
        {
            std::copy(b, e, begin());
            size_ = d;
        }
        
        return *this;
    }
    
    friend std::ostream& operator<<(std::ostream& os, buffer const& buf)
    {
        std::ostream_iterator<char> o(os);
        
        std::copy(buf.begin(), buf.end(), o);
        
        return os;
    }
    
    enum status { ok, bad, intermediate };
    status consume(const std::string& msg)
    {
        int len = msg.length();
        
        if (size_ + len > N) return bad;
        
        status r = intermediate;
        if (msg.back() == C) 
        {
            r = ok;
            len--;
        }
        
        std::memcpy(data_ + size_, msg.c_str(), len);
        size_ += len;
        
        return r;
    }
    
    status consume(int bytes)
    {
        if (size_ + bytes > N) return bad;
        
        status r = intermediate;
        size_ += bytes;
        if (data_[size_] == C) 
        {
            r = ok;
            size_--;
        }
        
        return r;
    }
    
    char data_[max_size];
    int size_;
};

using buffer_t = buffer<512>;

#endif