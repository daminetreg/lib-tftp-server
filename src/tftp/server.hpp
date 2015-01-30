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
#include <boost/format.hpp>
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

    //std::cout << "DEBUG: request_toparse:" << request_toparse << ":request_toparse" << std::endl;

    tftp::detail::possible_request req;
    auto iter = request_toparse.begin();
    auto end = request_toparse.end();
    tftp::detail::parser::request_grammar<decltype(iter)> grammar;

    bool r;
    try {
      r = parse(iter, end, grammar, req);
    } catch (qi::expectation_failure<decltype(iter)> const& x) {
      std::cout << "Error in request, expected: " << x.what_ << std::endl;
      throw;
    } 

    if (r) {
      return req;
    } else {
      throw std::runtime_error("Error in parsing request");
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

      server(boost::asio::io_service& io_service, short port = 69)
        : socket_(io_service, udp::endpoint(udp::v4(), port)) {
        waiting_for_requests();
      }

      void waiting_for_requests(const tftp::detail::possible_request* already_parsed_request = nullptr) {
        
        if (already_parsed_request != nullptr) {
          boost::apply_visitor(*this, *already_parsed_request);
        } else {

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
      }

      void operator()(tftp::detail::read_request request) {
        using namespace boost;
        std::cout << "INFO: Read request " << request << std::endl;

        if (request.data_mode != tftp::detail::mode::octet) {
          std::cout << "ERROR: transfer mode not supported : " << request.data_mode << std::endl;
          tftp::detail::error_response response(tftp::detail::error::undefined , "Only octect mode is supported by this Server");
          auto response_buffer = generate_response(response);
          socket_.async_send_to(
            asio::buffer(response_buffer.data(), response_buffer.size()), sender_endpoint_,
            [this, &response](system::error_code /*ec*/, std::size_t bytes_sent)
            {
              this->waiting_for_requests();
            });

        } else {


          filesystem::path current_file = filesystem::current_path() / request.filename;
          if (filesystem::exists(current_file)) {

            current_file_served = std::make_shared<std::fstream>(request.filename.data(), std::ifstream::in);
            current_file_served->exceptions(std::ifstream::badbit);

            // OACK the options if there is any
            if (!request.options.empty()) {

              tftp::detail::option_ack oack_response;
              for (auto option : request.options) {

                //XXX: Make this better, should also be case insensitive regarding RFC2348
                if (option.get<0>() == "blksize") { 
                  current_block_size_requested = std::atoi(option.get<1>().data());
                  oack_response.options.push_back(option);
                } else if (option.get<0>() == "tsize") {
                  uintmax_t filesize = filesystem::file_size(current_file);
                  tuple<std::string, std::string> tsize("tsize", str(format("%1%") % filesize));
                  oack_response.options.push_back(tsize);
                } else if (option.get<0>() == "timeout") {
                  current_timeout = std::chrono::seconds(std::atoi(option.get<1>().data()));
                  oack_response.options.push_back(option);
                }
              }

              auto response_buffer = generate_response(oack_response);
              //std::cout << "DEBUG: sending oack : " << response_buffer << std::endl; 
              socket_.async_send_to(
                asio::buffer(response_buffer.data(), response_buffer.size()), sender_endpoint_,
                [this](system::error_code /*ec*/, std::size_t /*bytes_sent*/) {

                  // Wait for ack of oack
                  socket_.async_receive_from(
                    boost::asio::buffer(data_, max_length), 
                    sender_endpoint_,
                    [this](boost::system::error_code ec, std::size_t bytes_recvd) {
                      if (!ec && bytes_recvd > 0) {
                        std::string request_toparse(data_, bytes_recvd);
                        auto req = parse_request(request_toparse);
                    
                        if ( boost::get<tftp::detail::acknowledgment>( &req ) != nullptr ) { // Is an ack
                            this->serve_current_file();
                        } else {
                          std::cout << "ERROR: The request of the client sent after our option_ack isn't an ack : " << req << std::endl; 
                          this->waiting_for_requests();
                        }

                      }
                    });
                }
              );

            } else {
              current_block_size_requested = tftp::detail::DEFAULT_DATA_BLOCK_SIZE;
              serve_current_file();
            }

          } else {

            std::cout << "ERROR: File not found " << request.filename << " cannot be found as " << current_file << std::endl;
            tftp::detail::error_response response(tftp::detail::error::file_not_found, current_file.generic_string() + " cannot be found");
            auto response_buffer = generate_response(response);
            socket_.async_send_to(
              asio::buffer(response_buffer.data(), response_buffer.size()), sender_endpoint_,
              [this, &response](system::error_code /*ec*/, std::size_t bytes_sent)
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
        std::string block(current_block_size_requested, '\0');

        //current_file_served->seekg((blocknum - 1) * tftp::detail::DATA_BLOCK_SIZE );
        current_file_served->read(const_cast<char*>(block.data()), block.size());

        bool last_block = false;
        if (!current_file_served->good()) { // Shrink to what was effectively read, meaning last packet.
          block.resize(current_file_served->gcount(), '\0');
          last_block = true;
          current_file_served.reset();
        }

        tftp::detail::data_response response(blocknum, block);
        auto response_buffer = generate_response(response);
        socket_.async_send_to(
          boost::asio::buffer(response_buffer.data(), response_buffer.size()),
          sender_endpoint_,
          [this, blocknum, last_block](boost::system::error_code ec, std::size_t bytes_sent) {
            this->wait_for_ack(blocknum, last_block);
        });

      }

      void wait_for_ack(uint16_t blocknum, bool last_block) {

        boost::asio::steady_timer timer(socket_.get_io_service());
          timer.expires_from_now(current_timeout); 
          timer.async_wait([this, blocknum](const boost::system::error_code& e) {
            if (e != boost::asio::error::operation_aborted) {
              // Timeout
              std::cout << "ERROR: Wating for block acknowledgment for block " << blocknum << " timed out." << std::endl;
              this->waiting_for_requests();
            }
          }
        );

        socket_.async_receive_from(
          boost::asio::buffer(data_, max_length), 
          sender_endpoint_,
          [this, blocknum, last_block](boost::system::error_code ec, std::size_t bytes_recvd) {
            if (!ec && bytes_recvd > 0) {

              std::string request_toparse(data_, bytes_recvd);

              try {
                auto req = parse_request(request_toparse);
            
                if ( boost::get<tftp::detail::acknowledgment>( &req ) != nullptr ) { // Is an ack
                  if (last_block) {
                    this->waiting_for_requests();
                  } else {
                    this->serve_current_file(blocknum + 1);
                  }

                } else {
                  std::cout << "ERROR: The request isn't an ack : " << req << std::endl; 
                  this->waiting_for_requests(&req);
                }
              } catch (std::runtime_error& err) {
                std::cout << "ERROR: strange request which cannot be parsed of size " << bytes_recvd << ", waiting again for ack." << std::endl;
                this->wait_for_ack(blocknum, last_block);
              }    

            }
          }
        );

      }

    private:
      udp::socket socket_;
      udp::endpoint sender_endpoint_;
      enum { max_length = 4096 };
      char data_[max_length];

      std::size_t current_block_size_requested = tftp::detail::DEFAULT_DATA_BLOCK_SIZE;
      std::chrono::seconds current_timeout = std::chrono::seconds(5);
      std::shared_ptr<std::fstream> current_file_served;
      
  };
 
}

#endif
