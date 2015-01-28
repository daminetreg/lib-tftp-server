# lib-tftp-server
I had to automate the flashing process of an embedded device, basically : read/write to a serial console to configure a bootloader to load our NAND image from TFTP and flash it down.
But there was no clean, modern C++, header only, moderate in terms of dependencies implementation of a library to launch  Trivial File Transfer Protocol server ( _c.f._ [RFC 1350](./rfc1350.txt) ) implementation.

So the idea with this server, is to provide other people in a need similar to mine with a simple tftp server that they can start from their application as a library.

## TFTP Support
This server isn't a fully complete implementation of TFTP, but is a quite trivial sufficient solution as it doesn't implements the strange things you don't need, see ( _c.f._ [RFC 1350](./rfc1350.txt) for the strange beast like netascii encoding and mail forwarding.  

**Supported Features:**

  * Supports Read Requests (RRQ, Download) of type octet/binary
  * Supports Data and ACK for files in the work dir of the application

**Easy-to-implement to be RFC1350 compliant:**
I welcome pull-requests or forks, as the code is small and the following might be implemented fast.

  * Support for Write Requests (WRQ, Upload) of type octet/binary : may be added in 15 lines of code.
  * Support for netascii transfers (I don't see why one would want this, as most clients like barebox doesn't even support them, as it requires further processing of bytes stream)
  * Support for resending unacked packet instead of aborting transfer

## How to use it ?

### Compiling
If you want to compile the demo tftp-server executable you can simply do the following : 

#### Dependencies
It only requires you to have Boost installed on your system : 

```sh
sudo apt-get install libboost-1.55-all libboost-1.55-dev
```

#### Build

```sh
mkdir build/
cd build/ && cmake ..
make
```

And then you to run a tftp-server on port 6565 you can run : 
```sh
build/src/tftp-server 6565
```

#### Reusing the library and starting it within your app
It's header only so you simply have to include the folder `src/` and then link to `boost_system, boost_filesystem`. 

To ease reuse I don't distribute a cmake package config file for the moment, however you can using make install to install the headers.
