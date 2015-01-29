# Design Choices
This documents presents why something is implemented in some way, so that people can get an understanding of what may be good/bad and changeable.

## server.hpp
This is just a spagheti code of boost::asio callbacks. One thing which should be done is implement a State Machine to dispatch the different request correctly and make a specific session handler.

However this state machine must be nice with clients, because tftp clients like the one of Barebox does send invalid empty request and errors before an ACK sometimes. So the spaghetti behaviour should be kept, it should just be made more clear in using something like Boost.MSM.

## tftp_packet_grammar.hpp
Implemented in terms of special structures and Boost.Spirit.Karma as well as Boost.Spirit.Qi to ease the readability of the byte layouting in comparison to the RFCs. It gets extremely performant thanks to compiler optinizations.
