`cdec` is a research platform for machine translation and similar structured prediction problems.

## Installation

Build `cdec`:

	autoreconf -ifv
	./configure
	make
	./tests/run-system-tests.pl

You will need the following libraries / tools:

- [Autoconf / Automake / Libtool](http://www.gnu.org/software/autoconf/)
    - Older versions of GNU autotools may not work properly.
- [Boost C++ libraries (version 1.44 or later)](http://www.boost.org/)
    - If you build your own boost, you _must install it_ using `bjam install`.
    - Older versions of Boost _may_ work, but problems have been reported with command line option parsing on some platforms with older versions.
- [GNU Flex](http://flex.sourceforge.net/)

## Further information

[For more information, refer to the cdec documentation](http://www.cdec-decoder.org)

