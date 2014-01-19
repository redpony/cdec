`cdec` is a research platform for machine translation and similar structured prediction problems.

## System requirements 

- A Linux or Mac OS X system
- A C++ compiler implementing the [C++-11 standard](http://www.stroustrup.com/C++11FAQ.html) <font color="red"><b>(NEW)</b></font>
    - Unfortunately, many systems have compilers that predate C++-11 support.
    - You may need to build your own C++ compiler or upgrade your operating system.
- [Boost C++ libraries (version 1.44 or later)](http://www.boost.org/)
    - If you build your own boost, you _must install it_ using `bjam install`.
    - Older versions of Boost _may_ work, but problems have been reported with command line option parsing on some platforms with older versions.
- [GNU Flex](http://flex.sourceforge.net/)

## Building from a downloaded archive

Instructions:

	./configure
	make
	./tests/run-system-tests.pl

## Building from a git clone

In addition to the standard `cdec` third party requirements, you will additionally need the following software:

- [Autoconf / Automake / Libtool](http://www.gnu.org/software/autoconf/)
    - Older versions of GNU autotools may not work properly.

Instructions:

	autoreconf -ifv
	./configure
	make
	./tests/run-system-tests.pl

## Further information

[For more information, refer to the `cdec` documentation](http://www.cdec-decoder.org)

