/*=============================================================================
  Copyright (c) 2015 Damien Buhl (alias daminetreg)

  Distributed under the Boost Software License, Version 1.0. (See accompanying
  file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#ifndef TFTP_DETAIL_TFTP_PACKET_GRAMMAR_HPP
#define TFTP_DETAIL_TFTP_PACKET_GRAMMAR_HPP

#include <cstdint>
#include <vector>
#include <string>

#include <boost/fusion/include/define_struct.hpp>
#include <boost/fusion/include/io.hpp>
#include <boost/fusion/include/map.hpp>
#include <boost/fusion/include/boost_tuple.hpp>

#include <boost/variant/variant.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_io.hpp>

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>

#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/phoenix/bind/bind_member_variable.hpp>


namespace tftp { namespace detail {

  const std::size_t DEFAULT_DATA_BLOCK_SIZE = 512;

  namespace opcode {
    const uint16_t read_request = 1;
    const uint16_t write_request = 2;
    const uint16_t data = 3;
    const uint16_t acknowledgment = 4;
    const uint16_t error = 5;
    const uint16_t option_ack = 6;
  }

  namespace error {
   const uint16_t undefined = 0;
   const uint16_t file_not_found = 1;
   const uint16_t access_violation = 2;
   const uint16_t disk_full = 3;
   const uint16_t illegal_operation = 4;
   const uint16_t unknown_transfer_id = 5;
   const uint16_t file_already_exists = 6;
   const uint16_t no_such_user = 7;
   const uint16_t unsupported_option = 8;
  }

  enum class mode {
    netascii,
    octet,
    mail
  };

  std::ostream& operator<<(std::ostream& os, const mode& m) {

    switch (m) {
      case mode::netascii:
        os << "netascii";
      break;
      case mode::octet:
        os << "octet";
      break;
      case mode::mail:
        os << "mail";
      break;
    }

    return os;
  }


  // For BOOST_FUSION_DEFINE_STRUCT below load via ADL the operators 
  using boost::fusion::operator<<;

  typedef std::vector< boost::tuple<std::string, std::string> > tftp_options;
}}

BOOST_FUSION_DEFINE_STRUCT(
    (tftp)(detail), read_request,
    (std::string, filename)
    (tftp::detail::mode, data_mode)
    (tftp::detail::tftp_options, options)
)

BOOST_FUSION_DEFINE_STRUCT(
    (tftp)(detail), write_request,
    (std::string, filename)
    (tftp::detail::mode, data_mode)
)

BOOST_FUSION_DEFINE_STRUCT(
    (tftp)(detail), acknowledgment,
    (uint16_t, blocknum)
)

BOOST_FUSION_DEFINE_STRUCT(
    (tftp)(detail), data_response,
    (uint16_t, blocknum)
    (std::string, data)
)

BOOST_FUSION_DEFINE_STRUCT(
    (tftp)(detail), error_response,
    (uint16_t, error_code)
    (std::string, error_msg)
)

BOOST_FUSION_DEFINE_STRUCT(
    (tftp)(detail), option_ack,
    (tftp::detail::tftp_options, options)
)

namespace tftp { namespace detail {

  typedef boost::variant<
    read_request,
    write_request,
    acknowledgment
  > possible_request;

  typedef boost::variant<
    data_response,
    error_response,
    option_ack
  > possible_response;

}}

namespace tftp { namespace detail { namespace parser {

  using namespace boost::spirit;
  using namespace boost::spirit::qi;
  using namespace boost::phoenix;

  using namespace tftp::detail;

  template<typename Iterator>
  struct request_grammar
    : grammar<Iterator, possible_request()> {

    /**
     * Pases a request packet 
     */
    rule<Iterator, possible_request()> request;

    /**
     * Parses an ascii name
     */
    rule<Iterator, std::string()> name;

    /**
     * Parses mode (netascii, octet, mail)
     */
    rule<Iterator, mode()> mode_name;

    /**
     * Implements options list of RFC2347 (used by barebox namely)
     */
    rule<Iterator, tftp_options()> options;

    request_grammar() :
      request_grammar::base_type(request) {
        request = 
          (big_word(opcode::read_request) > name > lit('\0') > mode_name > lit('\0') > options )[_val = construct<read_request>(_1, _2, _3)]
          | (big_word(opcode::write_request) > name > lit('\0') > mode_name > lit('\0') )[_val = construct<write_request>(_1, _2)]
          | (big_word(opcode::acknowledgment) > big_word)[_val = construct<acknowledgment>(_1)]
        ;

      name = *(char_ - char_('\0'));

      mode_name = no_case[lit("netascii")][_val = construct<mode>(mode::netascii)] 
        | no_case[lit("octet")][_val = construct<mode>(mode::octet)]
        | no_case[lit("mail")][_val = construct<mode>(mode::mail)];

      options = *(name >> lit('\0') > name >> lit('\0'));

      // Names for better error report (e.g. expected filename but got...)
      request.name("request");
      name.name("name");
      mode_name.name("mode_name");
      options.name("options");
    }

  };


}}}

namespace tftp { namespace detail { namespace generator {
  
  using namespace boost::spirit;
  using namespace boost::spirit::karma;
  using namespace boost::phoenix;

  using namespace tftp::detail;

  template<typename OutputIterator>
  struct response_grammar 
    : grammar<OutputIterator, possible_response()> {

    /**
     * Generates a response packet
     */
    rule<OutputIterator, possible_response()> response;

    rule<OutputIterator, data_response()> data;
    rule<OutputIterator, error_response()> error;
    rule<OutputIterator, option_ack()> oack;

    response_grammar() :
      response_grammar::base_type(response) {
        
        response = data | error | oack;

        data = big_word(opcode::data) << big_word << *char_;

        error = big_word(opcode::error) << big_word << *char_ << lit('\0');

        oack = big_word(opcode::option_ack) << *( *char_  << lit('\0') << *char_ << lit('\0') );

        // Names for better error report
        response.name("response");
        data.name("data");
        error.name("error");
        oack.name("error");
    }

  };



}}}

#endif
