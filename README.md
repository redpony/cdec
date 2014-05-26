`cdec` is a research platform for machine translation and similar structured prediction problems.

[![Build Status](https://travis-ci.org/redpony/cdec.svg?branch=master)](https://travis-ci.org/redpony/cdec)

## System requirements 

- A Linux or Mac OS X system
- A C++ compiler implementing the [C++-11 standard](http://www.stroustrup.com/C++11FAQ.html) <font color="red"><b>(NEW)</b></font>
    - Unfortunately, many systems have compilers that predate C++-11 support.
    - You may need to build your own C++ compiler or upgrade your operating system's.
- [Boost C++ libraries (version 1.44 or later)](http://www.boost.org/)
    - If you build your own boost, you _must install it_ using `bjam install`.
    - Older versions of Boost _may_ work, but problems have been reported with command line option parsing on some platforms with older versions.
- [GNU Flex](http://flex.sourceforge.net/)

## Building from a downloaded archive

If your system contains the required tools and libraries in the usual places, you should be able to build as simply as:

    ./configure
    make -j4
    ./tests/run-system-tests.pl

## Building from a git clone

In addition to the standard `cdec` third party software requirements, you will additionally need the following software to work with the `cdec` source code directly from git:

- [Autoconf / Automake / Libtool](http://www.gnu.org/software/autoconf/)
    - Older versions of GNU autotools may not work properly.

Instructions:

    autoreconf -ifv
    ./configure
    make -j4
    ./tests/run-system-tests.pl

## Further information

[For more information, refer to the `cdec` documentation](http://www.cdec-decoder.org)

## Citation

If you make use of cdec, please cite:

C. Dyer, A. Lopez, J. Ganitkevitch, J. Weese, F. Ture, P. Blunsom, H. Setiawan, V. Eidelman, and P. Resnik. cdec: A Decoder, Alignment, and Learning Framework for Finite-State and Context-Free Translation Models. In *Proceedings of ACL*, July, 2010. [[bibtex](http://www.cdec-decoder.org/cdec.bibtex.txt)] [[pdf](http://www.aclweb.org/anthology/P/P10/P10-4002.pdf)]
