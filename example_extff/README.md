This is an example of an _external_ feature function which is loaded as a dynamically linked library at run time to compute feature functions over derivations in a hypergraph. To load feature external feature functions, you can specify them in your `cdec.ini` configuration file as follows:

	feature_function=External /path/to/libmy_feature.so

Any extra options are passed to the external library.

*Note*: the build system uses [GNU Libtool](http://www.gnu.org/software/libtool/) to create the shared library. This may be placed in a hidden directory called `./libs`.

