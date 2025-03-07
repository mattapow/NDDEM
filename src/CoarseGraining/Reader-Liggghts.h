#include "Reader.h"

#include <cstdlib>
#include <cstdio>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <cmath>

#include "gzip.hpp"
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/stream.hpp>
#include "Typedefs.h"

class LiggghtsReader : public Reader {
public:
    LiggghtsReader(std::string ppath) : path(ppath) 
    {
        open(path) ; 
    }
    ~LiggghtsReader() {
        if ( in!= nullptr ) {delete(in) ; in = nullptr ; }
        if (filt_in != nullptr) {delete(filt_in) ; filt_in = nullptr ; }
        if (filt_in_seekable != nullptr) {delete(filt_in_seekable) ; filt_in_seekable = nullptr;}  
        if (file_in != nullptr)
        {
            file_in->close() ; 
            delete(file_in) ;
            file_in = nullptr ; 
        }
    }

    std::istream *in = nullptr ;

    int curts = -1, actualts=0 ; 
    int Nitem ; 

    std::vector <std::string> fields ;
    v2d data, tdata ;
    v1d dataextra ;

    // Functions
    int open(string path) ;
    int reset() ; 
    
    //int read_full_ts(bool keep) ;
    //int set_data(struct Data & D, std::map<string,size_t> extrafieldmap) ;
    std::vector<vector<double>> get_bounds () ;
    int get_numts () ;
    int read_timestep (int ts) ; 
    int get_num_particles () {return Nitem ; }
    int get_num_contacts () {return Nitem ; }

    vector <vector <double> > boundaries = vector<vector<double>>(3,vector<double>(3,0)) ;
    vector<bool> periodicity = vector<bool>(3,false) ;
    
    std::map <std::string, std::string> cfmapping ;

private:
    boost::iostreams::filtering_streambuf<boost::iostreams::input> *filt_in;
    boost::iostreams::filtering_streambuf<boost::iostreams::input_seekable> *filt_in_seekable;
    ifstream * file_in = nullptr ;
    std::string path ; 
    int read_timestep_impl (int ts, bool skip=false) ; 
    virtual int do_post_read() {return 0 ;} 
} ;
//-------------------------------------------
class LiggghtsReader_particles: public LiggghtsReader {
public: 
    LiggghtsReader_particles(std::string ppath) : LiggghtsReader(ppath) {}
    
    bool has_id_data = false ; 
    double * get_data (DataValue datavalue, int dd) {
        switch(datavalue) {
            case DataValue::radius : return &(data[0][0]) ;
            case DataValue::mass : return &(data[1][0] ) ;
            case DataValue::Imom : return &(data[2][0] ) ;
            
            case DataValue::pos : return &(data[3+dd][0] ) ;
            case DataValue::vel : return &(data[6+dd][0] ) ;
            case DataValue::omega : return &(data[9+dd][0] ) ;
            
            default : return (nullptr) ; 
        }
    } 
    
private:
    int do_post_read() ;
} ;
//-------------------------------------------
class LiggghtsReader_contacts : public LiggghtsReader {
public:
    LiggghtsReader_contacts(std::string ppath, Reader *d, std::map <std::string, std::string> columnmap) : LiggghtsReader(ppath), dump(dynamic_cast<LiggghtsReader_particles*>(d)) {cfmapping=columnmap;} ; 
    double * get_data (DataValue datavalue, int dd) {
        switch(datavalue) {
            case DataValue::id1 : return &(data[0][0]) ;
            case DataValue::id2 : return &(data[1][0] ) ;
            
            case DataValue::pospq : return &(data[2+dd][0] ) ;
            case DataValue::lpq   : return &(data[5+dd][0] ) ;
            case DataValue::fpq   : return &(data[8+dd][0] ) ;
            case DataValue::mpq   : return &(data[11+dd][0] ) ;
            case DataValue::mqp   : return &(data[14+dd][0] ) ;
            
            default : return (nullptr) ; 
        }
    }
private:
    LiggghtsReader_particles *dump ; 
    int do_post_read() ;
} ; 
