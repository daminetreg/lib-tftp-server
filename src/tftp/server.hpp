/*=============================================================================
  Copyright (c) 2015 Damien Buhl (alias daminetreg)

  Distributed under the Boost Software License, Version 1.0. (See accompanying
  file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#ifndef TFTP_SERVER_HPP
#define TFTP_SERVER_HPP

#include <stdexcept>
#include <memory>
#include <string>
#include <vector>
#include <cstdlib>
#include <iterator>
#include <iostream>
#include <fstream>

#include <chrono>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include <boost/algorithm/string/replace.hpp>

#include <tftp/detail/tftp_packet_grammar.hpp>


#include <vector>

namespace std {
  template < class T >
  ostream& operator << (ostream& os, const vector<T>& v) 
  {
      os << "[";
      for (typename vector<T>::const_iterator ii = v.begin(); ii != v.end(); ++ii)
      {
          os << " " << *ii;
      }
      os << "]";
      return os;
  }
}

namespace tftp {

  using boost::asio::ip::udp;

  inline tftp::detail::possible_request parse_request(const std::string& request_toparse) {
    using namespace boost::spirit;

    tftp::detail::possible_request req;
    auto iter = request_toparse.begin();
    auto end = request_toparse.end();
    tftp::detail::parser::request_grammar<decltype(iter)> grammar;

    bool r;
    r = parse(iter, end, grammar, req);

    if (r) {
      return req;
    } else {
      throw std::runtime_error("Wrong request.");
    }
  }

  inline std::string generate_response(const tftp::detail::possible_response& response) {
    using namespace boost::spirit;

    std::string response_buffer;
    std::back_insert_iterator<std::string> sink(response_buffer);
    tftp::detail::generator::response_grammar<decltype(sink)> grammar;
    
    bool r;
    r = generate(sink, grammar, response);

    if (r) {
      return response_buffer;
    } else {
      throw std::runtime_error("Error generating response.");
    }     

  }

  class server : public boost::static_visitor<> {
    public:

      server(boost::asio::io_service& io_service, short port)
        : socket_(io_service, udp::endpoint(udp::v4(), port)) {
        waiting_for_requests();
      }

      void waiting_for_requests() {
        socket_.async_receive_from(
          boost::asio::buffer(data_, max_length), sender_endpoint_,
          [this](boost::system::error_code ec, std::size_t bytes_recvd) {

            if (!ec && bytes_recvd > 0) {
              std::string request_toparse(data_, bytes_recvd);
              tftp::detail::possible_request req = parse_request(request_toparse);
              boost::apply_visitor(*this, req);
            }

          });
      }

      void operator()(tftp::detail::read_request request) {
        std::cout << "INFO: Read request " << request << std::endl;

        if (request.data_mode != tftp::detail::mode::octet) {
          std::cout << "ERROR: transfer mode not supported : " << request.data_mode << std::endl;
          tftp::detail::error_response response(tftp::detail::error::undefined , "Only octect mode is supported by this Server");
          auto response_buffer = generate_response(response);
          socket_.async_send_to(
            boost::asio::buffer(response_buffer.data(), response_buffer.size()), sender_endpoint_,
            [this, &response](boost::system::error_code /*ec*/, std::size_t bytes_sent)
            {
              this->waiting_for_requests();
            });

        } else {

          boost::filesystem::path current_file = boost::filesystem::current_path() / request.filename;
          if (boost::filesystem::exists(current_file)) {
            current_file_served = std::make_shared<std::fstream>(request.filename.data(), std::ifstream::in);
            current_file_served->exceptions(std::ifstream::badbit);
            serve_current_file();
          } else {

            std::cout << "ERROR: File not found " << request.filename << " cannot be found as " << current_file << std::endl;
            tftp::detail::error_response response(tftp::detail::error::file_not_found, current_file.generic_string() + " cannot be found");
            auto response_buffer = generate_response(response);
            socket_.async_send_to(
              boost::asio::buffer(response_buffer.data(), response_buffer.size()), sender_endpoint_,
              [this, &response](boost::system::error_code /*ec*/, std::size_t bytes_sent)
              {
                this->waiting_for_requests();
              });
          }

        } 

      }

      void operator()(tftp::detail::write_request request) {
        std::cout << "NOT IMPLEMENTED: Write request " << request << std::endl;
        tftp::detail::error_response response(tftp::detail::error::illegal_operation, "write_request not implemented");
        auto response_buffer = generate_response(response);
        socket_.async_send_to(
          boost::asio::buffer(response_buffer.data(), response_buffer.size()), sender_endpoint_,
          [this, &response](boost::system::error_code /*ec*/, std::size_t bytes_sent)
          {
            this->waiting_for_requests();
          });
      }

      void operator()(tftp::detail::acknowledgment ack) {
        std::cout << "INFO: SPURIOUS Acknowledgment " << ack << std::endl;
        tftp::detail::error_response response(tftp::detail::error::illegal_operation, "acknowledgment while no transfer running");
        auto response_buffer = generate_response(response);
        socket_.async_send_to(
          boost::asio::buffer(response_buffer.data(), response_buffer.size()), sender_endpoint_,
          [this, &response](boost::system::error_code /*ec*/, std::size_t bytes_sent)
          {
            this->waiting_for_requests();
          });
      }

      void serve_current_file(uint16_t blocknum = 1) {
        std::string block(tftp::detail::DATA_BLOCK_SIZE, '\0');

        //current_file_served->seekg((blocknum - 1) * tftp::detail::DATA_BLOCK_SIZE );
        current_file_served->read(const_cast<char*>(block.data()), block.size());

        bool last_block = false;
        if (!current_file_served->good()) { // Shrink to what was effectively read, meaning last packet.
          block.resize(current_file_served->gcount(), '\0');
          last_block = true;
          current_file_served.reset();
        }

        //tftp::detail::data_response response(blocknum, block);
        //auto response_buffer = generate_response(response);


        tftp::detail::data_response response(blocknum);//, block);
        auto prefix_response_buffer = generate_response(response);

        std::string response_buffer(prefix_response_buffer.size() + block.size(), '\0');
        memcpy(const_cast<char*>(response_buffer.data()), prefix_response_buffer.data(), prefix_response_buffer.size());
        memcpy((void*)(ptrdiff_t(const_cast<char*>(response_buffer.data())) + ptrdiff_t(prefix_response_buffer.size())), block.data(), block.size());


        socket_.async_send_to(
          boost::asio::buffer(response_buffer.data(), response_buffer.size()),
          sender_endpoint_,
          [this, blocknum, last_block](boost::system::error_code ec, std::size_t bytes_sent) {

            boost::asio::steady_timer timer(socket_.get_io_service());
            timer.expires_from_now(std::chrono::seconds(5)); 
            timer.async_wait([this, blocknum](const boost::system::error_code& e) {
              if (e != boost::asio::error::operation_aborted) {
                // Timeout
                std::cout << "ERROR: Wating for block acknowledgment for block " << blocknum << " timed out." << std::endl;
                this->waiting_for_requests();
              }
            });

            socket_.async_receive_from(
              boost::asio::buffer(data_, max_length), 
              sender_endpoint_,
              [this, blocknum, last_block](boost::system::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                  std::string request_toparse(data_, bytes_recvd);
                  auto req = parse_request(request_toparse);
              
                  if ( boost::get<tftp::detail::acknowledgment>( &req ) != nullptr ) { // Is an ack
                    if (last_block) {
                      this->waiting_for_requests();
                    } else {
                      this->serve_current_file(blocknum + 1);
                    }

                  } else {
                    std::cout << "ERROR: The request isn't an ack : " << req << std::endl; 
                    this->waiting_for_requests();
                  }

                }
              });

        });

      }

    private:
      udp::socket socket_;
      udp::endpoint sender_endpoint_;
      enum { max_length = 1024 };
      char data_[max_length];

      std::shared_ptr<std::fstream> current_file_served;
  };
 
}

#endif
