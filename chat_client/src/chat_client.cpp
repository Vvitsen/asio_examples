/*
 * chat_client.cpp
 *
 */

#include <cstdlib>
#include <deque>
#include <iostream>
#include <boost/bind.hpp>
#include <asio.hpp>
#include "chat_message.hpp"

using asio::ip::tcp;

typedef std::deque<chat_message> chat_message_queue;

class chat_client
{
private:
  asio::io_service& io_context_;
  tcp::socket socket_;
  chat_message read_msg_;
  chat_message_queue write_msgs_;

public:
//  chat_client(asio::io_service& io_context,
//		  	  const tcp::resolver::results_type& endpoints)
//    		: io_context_(io_context),
//    		  socket_(io_context)
//  {
//    asio::async_connect(socket_, endpoints,
//        boost::bind(&chat_client::handle_connect, this,
//          asio::placeholders::error));
//  }

  chat_client(asio::io_service& io_context,
		  	  const tcp::resolver::iterator& begin,
		  	  const tcp::resolver::iterator& end)
    		: io_context_(io_context),
    		  socket_(io_context)
  {
    asio::async_connect(socket_, begin,end,
        boost::bind(&chat_client::handle_connect, this,
          asio::placeholders::error));
  }

  void write(const chat_message& msg)
  {
//    asio::post(io_context_,
//        boost::bind(&chat_client::do_write, this, msg));
	  this->io_context_.post(boost::bind(&chat_client::do_write, this, msg));
  }

  void close()
  {
//    asio::post(io_context_,
//        boost::bind(&chat_client::do_close, this));
	  this->io_context_.post(boost::bind(&chat_client::do_close, this));
  }

private:

  void handle_connect(const asio::error_code& error)
  {
    if (!error)
    {
      asio::async_read(socket_,
          asio::buffer(read_msg_.data(), chat_message::header_length),
          boost::bind(&chat_client::handle_read_header, this,
            asio::placeholders::error));
    }
  }

  void handle_read_header(const asio::error_code& error)
  {
    if (!error && read_msg_.decode_header())
    {
      asio::async_read(socket_,
          asio::buffer(read_msg_.body(), read_msg_.body_length()),
          boost::bind(&chat_client::handle_read_body, this,
            asio::placeholders::error));
    }
    else
    {
      do_close();
    }
  }

  void handle_read_body(const asio::error_code& error)
  {
    if (!error)
    {
      std::cout.write(read_msg_.body(), read_msg_.body_length());
      std::cout << "\n";
      asio::async_read(socket_,
          asio::buffer(read_msg_.data(), chat_message::header_length),
          boost::bind(&chat_client::handle_read_header, this,
            asio::placeholders::error));
    }
    else
    {
      do_close();
    }
  }

  void do_write(chat_message msg)
  {
    bool write_in_progress = !write_msgs_.empty();
    write_msgs_.push_back(msg);
    if (!write_in_progress)
    {
      asio::async_write(socket_,
          asio::buffer(write_msgs_.front().data(),
            write_msgs_.front().length()),
          boost::bind(&chat_client::handle_write, this,
            asio::placeholders::error));
    }
  }

  void handle_write(const asio::error_code& error)
  {
    if (!error)
    {
      write_msgs_.pop_front();
      if (!write_msgs_.empty())
      {
        asio::async_write(socket_,
            asio::buffer(write_msgs_.front().data(),
              write_msgs_.front().length()),
            boost::bind(&chat_client::handle_write, this,
              asio::placeholders::error));
      }
    }
    else
    {
      do_close();
    }
  }

  void do_close()
  {
    socket_.close();
  }
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: chat_client <host> <port>\n";
      return 1;
    }

    asio::io_service io_context;

    tcp::resolver resolver(io_context);
//    tcp::resolver::results_type endpoints = resolver.resolve(argv[1], argv[2]);
    tcp::resolver::query query(argv[1], argv[2]);
    tcp::resolver::iterator begin = resolver.resolve(query), end;
//    chat_client c(io_context, endpoints);
    chat_client c(io_context, begin, end);

    asio::thread t( boost::bind(static_cast<long unsigned int(asio::io_service::*)()> (&asio::io_service::run), &io_context) );

    char line[chat_message::max_body_length + 1];
    while (std::cin.getline(line, chat_message::max_body_length + 1))
    {
      using namespace std; // For strlen and memcpy.
      chat_message msg;
      msg.body_length(strlen(line));
      memcpy(msg.body(), line, msg.body_length());
      msg.encode_header();
      c.write(msg);
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


