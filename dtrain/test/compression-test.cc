#include <iostream>
#include <fstream>
#include <boost/iostreams/device/file.hpp> 
#include <boost/iostreams/filter/zlib.hpp>
//#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>



using namespace boost::iostreams;
using namespace std;

int main() 
{
    

    //ofstream raw("out-raw");
    filtering_ostream out_z;
    out_z.push(gzip_compressor());
    //out_gz.push(gzip_compressor());
    out_z.push(file_sink("out-z", std::ios::binary));
    //out_gz.push(file_sink("out-gz", std::ios::binary));
    for ( size_t i = 0; i < 10; i++) {
        out_z << "line #" << i << endl;
        //out_gz << "line #" << i << endl;
        //out_bz << "line #" << i << endl;
        //raw << "line #" << i << endl;
    }
    //    flush(out);
    close(out_z);
    //close(out_gz);
    //close(out_bz);
    //raw.close();

    for (size_t i = 0; i < 5; i++) {
    ifstream file("out-z", ios_base::in | ios_base::binary);
    filtering_istream in;
    in.push(gzip_decompressor());
    in.push(file);
    string s;
    while (getline(in, s)) {
        cout << s << endl;
    }
    file.close();
    }

}

