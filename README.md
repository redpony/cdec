`cdec` is a research platform for machine translation and similar structured prediction problems.

[![Build Status](https://travis-ci.org/redpony/cdec.svg?branch=master)](https://travis-ci.org/redpony/cdec)

## System requirements 

- A Linux or Mac OS X system
- A C++ compiler implementing at least the [C++-11 standard](http://www.stroustrup.com/C++11FAQ.html)
    - Some systems may have compilers that predate C++-11 support.
    - You may need to build your own C++ compiler or upgrade your operating system's.
- [Boost C++ libraries (version 1.44 or later)](http://www.boost.org/)
    - If you build your own boost, you _must install it_ using `bjam install` (to install it into a customized location use `--prefix=/path/to/target`).
- [GNU Flex](http://flex.sourceforge.net/)
- [cmake](http://www.cmake.org/) - <font color="red"><b>(NEW)</b></font>

## Building the software

Build instructions:

    cmake .
    make -j4
    make test
    ./tests/run-system-tests.pl

## Further information

[For more information, refer to the `cdec` documentation](http://www.cdec-decoder.org)

## Citation

If you make use of cdec, please cite:

C. Dyer, A. Lopez, J. Ganitkevitch, J. Weese, F. Ture, P. Blunsom, H. Setiawan, V. Eidelman, and P. Resnik. cdec: A Decoder, Alignment, and Learning Framework for Finite-State and Context-Free Translation Models. In *Proceedings of ACL*, July, 2010. [[bibtex](http://www.cdec-decoder.org/cdec.bibtex.txt)] [[pdf](http://www.aclweb.org/anthology/P/P10/P10-4002.pdf)]
