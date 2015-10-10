### Disclaimer: MASE:BC2 was not written by me and does only work with legitimate copies of BFBC2! It is intended for use in your local network only.

Demo video: https://www.youtube.com/watch?v=pA8Fgv647Mk

A compiled version including a readme for with setup instructions and the required client dll can be downloaded here: http://www.mediafire.com/download/b81iiq4ww3lx4ph/mase_bc2_v9.zip

This repository is mostly intended for archive purposes and a nice way to browser the source code for those interested.

# Copy of the relevant part of the included README.txt:

Requirements:
-------------
- Visual Studio 2010 or higher (Windows), gcc 4.7 or higher (Linux)
- Boost C++ libraries v1.54.0 (you might have build them with the parameter "cxxflags=-std=gnu++11" in Linux to avoid some linking issues with boost::filesystem)
- OpenSSL v1.0.1j
(newer versions might work too but the ones listed here were used to build the latest version of the emulator)

Usage:
------
- Project files are included with the source files in the "src" folder 
- [Windows] mase_bc2.sln/.vcxproj/.vcxproj.filters for VS2010
- [Linux] mase_bc2.cbp for C::B (should also be possible to generate a makefile from it)
- To be able to properly build it you'll need to change the Include and Linker directories in the project settings first, replace all occurrences of "$(BOOST_1_54_DIR)" and "$(OPEN_SSL_DIR)" with the locations where you installed these libraries

Final Notes:
------------
I have written ~95% of the code myself (regarding the source files that I included here, code from the external libraries doesn't count of course), the rest I either copied 1:1 or just slightly modified it for my needs.
The according code sections in the files are:
- base64 de/encryption (in Base64.cpp/h, credits for the original author are included in the .cpp file)
- Packet de/encryption and SSL certificate information (in SSL_cert.h and TcpConnections(SSL).cpp), I originally took it from the v10.0 emulator source from Domo/Freaky123 but I it seems they took it from aluigi's EAList tool (http://aluigi.altervista.org/papers/ealist.zip), so I guess credits go to him?
- Http server code, mostly taken from the boost::asio example page (http://www.boost.org/doc/libs/1_54_0/doc/html/boost_asio/examples/cpp11_examples.html), only dumped it down and slightly modified it for my needs

Also I would like to mention that when I wrote this project, I was rather new to coding bigger stuff in general (though it certainly was a great experience for me) and I would probably do quite a few things differently nowadays if I had the time and motivation to do it.
So if you consider any of the code that I have written as bad/terrible regarding performance/structure/functioning then feel free to change anything you want or if thats too much work for you, just don't use the entire emulator and look for a different solution (I'm not forcing you to use it).
With that being said though, if you do change something in the code and release it somewhere, I would very much appreciate it if you would at least credit me for my work :)

# CREDITS

Developers (Emulator+Hook):
- Triver

Packet Logging and Server stuff:
- Triver
- Rodney (after Alpha 0.8)
- bcool (up to Alpha 0.8)

Beta Testers:
- Rodney
- Jack
- Michl2007
- Happy Chicken

Special Thanks:
- Domo and Freaky123 (for sharing the Server Files and the v10.0 source code)
- Aluigi (for the tools that made it possible to view and record the BF:BC2 network traffic)
- DICE (for making the game)
