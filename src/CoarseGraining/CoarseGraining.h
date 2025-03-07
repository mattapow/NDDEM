#include <cstdlib>
#include <cstdio>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <math.h>
#include <sstream>

#ifdef EMSCRIPTEN
    #include <emscripten.h>
    #include <emscripten/bind.h>
    using namespace emscripten ;
#endif

#include "gzip.hpp"
//#include "termcolor.hpp"
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/stream.hpp>
#include "json.hpp"
#include "Typedefs.h"
#include "Coarsing.h"
#include "Reader.h"
#include "Reader-Liggghts.h"
#include "Reader-NDDEM.h"
#include "Reader-interactive.h"
#include "Reader-Mercury.h"
#include "Parameters.h"

class CoarseGraining {
public:
    CoarseGraining() {}
    Param   P ;
    Coarsing * C = nullptr;
    Pass pipeline ;

    void setup_CG () ;

    int process_timestep (int ts, bool allow_avg_fluct=false) ;
    int process_fluct_from_avg() ;
    void process_all ();
    void write () ;
    std::vector<double> get_result(int ts, std::string field, int component) ;
    std::vector<double> get_gridinfo() ;

    // Expose functions from member classes for js access
    void param_from_json_string (std::string param)  { json jsonparam =json::parse(param) ; return P.from_json(jsonparam) ; }
    std::vector<std::vector<double>> param_get_bounds (int file = 0) {return P.files[file].reader->get_bounds() ; }
    int param_get_numts(int file = 0) {return P.files[file].reader->get_numts(); }
    void debug () {/*char resc[5000] ; sprintf(resc, "%d %X", P.files.size(), P.files[0].reader) ; return resc ; */printf("BLAAAA\n") ; }
    int param_read_timestep(int n) {return P.read_timestep(n) ; }
    void param_post_init () {return P.post_init() ; }

    /*std::string testing(std::string input) {
        FILE * in = fopen(input.c_str(), "r") ;
        printf("[%s]", input.c_str()) ;
        if (in != nullptr)
        {
            printf("Starting reading\n") ;
            while (!feof(in))
                fscanf(in,"%*c") ;
            fclose(in) ;
            printf("Finished reading\n") ;
        }
        return(input) ;
    } */

} ;
//=========================================================

void CoarseGraining::setup_CG ()
{
    //P.verify() ;
    if (C != nullptr)
        delete C ;

    C = new Coarsing (P.dim, P.boxes, P.boundaries, P.maxT) ;
    printf("%d\n", P.maxT) ;
    for (auto i : P.extrafields)
        C->add_extra_field(i.name, i.order, i.type) ;
    printf("%d \n", static_cast<int>(P.window)) ;
    if (P.window == Windows::LucyND_Periodic)
        C->setWindow(P.window, P.windowsize, P.periodicity, P.boxes, P.delta) ;
    else
        C->setWindow(P.window,P.windowsize) ;
    pipeline = C->set_flags(P.flags) ;
    auto extrafieldmap = C->grid_setfields() ;
    C->grid_neighbour() ;

    C->cT=-1 ;
}
//-----------------------------------------
int CoarseGraining::process_timestep (int ts_abs, bool allow_avg_fluct)
{
    int ts = ts_abs - P.skipT ;

    if ((P.timeaverage == AverageType::Intermediate   || P.timeaverage == AverageType::Both) &&
        (((pipeline & Pass::VelFluct) && !C->hasvelfluct) ||
            ((pipeline & Pass::RotFluct) && !C->hasrotfluct)))
        {

            if (!allow_avg_fluct)
            {
                printf("ERR: single timestep cannot run when intermediary average is required") ;
                return -1 ;
            }
            else
                process_fluct_from_avg() ;
        }

    bool avg=false ;
    if (P.timeaverage == AverageType::Intermediate || P.timeaverage == AverageType::Both) avg=true ;

    P.read_timestep(ts+P.skipT) ;
    C->cT = ts ;
    P.set_data (C->data) ;

    if (pipeline & Pass::Pass1) C->pass_1() ;
    if (pipeline & Pass::VelFluct) C->compute_fluc_vel (avg) ;
    if (pipeline & Pass::RotFluct) C->compute_fluc_rot (avg) ;
    if (pipeline & Pass::Pass2) C->pass_2() ;
    if (pipeline & Pass::Pass3) C->pass_3() ;
    if (pipeline & Pass::Pass4) C->pass_4() ;
    if (pipeline & Pass::Pass5) C->pass_5() ;
    return 0 ;
}
//------------------------------------------------------------
int CoarseGraining::process_fluct_from_avg()
{
    for (int ts=0 ; ts<P.maxT ; ts++)
    {
        P.read_timestep(ts+P.skipT) ;
        C->cT = ts ;
        P.set_data (C->data) ;
        if (pipeline & Pass::Pass1) C->pass_1() ;
        printf("\r") ;
    }
    if (P.timeaverage == AverageType::Intermediate   || P.timeaverage == AverageType::Both)
        C->mean_time(true) ;
    return 0 ;
}
//----------------------------------------------------------
void CoarseGraining::process_all ()
{
    for (int ts=0 ; ts<P.maxT ; ts++)
    {
        printf("\r%d ", ts) ;
        process_timestep(ts+P.skipT, true) ;
    }
    if (P.timeaverage == AverageType::Final || P.timeaverage == AverageType::Both)
        C->mean_time(false) ;
}
//------------------------------------------------------
void CoarseGraining::write ()
{
    if (std::find(P.saveformat.begin(), P.saveformat.end(), "netCDF")!=P.saveformat.end())   C->write_netCDF(P.save) ;
    if (std::find(P.saveformat.begin(), P.saveformat.end(), "vtk")!=P.saveformat.end()) C->write_vtk (P.save) ;
    if (std::find(P.saveformat.begin(), P.saveformat.end(), "mat")!=P.saveformat.end()) C->write_matlab(P.save, true) ;
    if (std::find(P.saveformat.begin(), P.saveformat.end(), "numpy")!=P.saveformat.end()) C->write_numpy(P.save, true) ;
}
//------------------------------------------------------
std::vector<double> CoarseGraining::get_result(int ts_abs, std::string field, int component)
{
 int ts = ts_abs - P.skipT ;
 if (ts != C->cT) {printf("The requested timestep has not been processed.\n"); return {} ; }
 int idx = C->get_id(field) ;
 idx += component ;
 std::vector <double> res ;
 res.resize(C->CGP.size()) ;
 for (size_t i = 0 ; i<res.size() ; i++)
     res[i]=C->CGP[C->idx_FastFirst2SlowFirst(i)].fields[C->cT][idx] ;
 return res ;
}
//-----------------------------------------------------
std::vector<double> CoarseGraining::get_gridinfo()
{
    std::vector<double> res ;
    //origin
    res.push_back(C->CGP[0].location[0]) ;
    res.push_back(C->CGP[0].location[1]) ;
    res.push_back(C->CGP[0].location[2]) ;
    //spacing
    res.push_back(C->dx[0]) ;
    res.push_back(C->dx[1]) ;
    res.push_back(C->dx[2]) ;
    //dimensions
    res.push_back(C->npt[0]) ;
    res.push_back(C->npt[1]) ;
    res.push_back(C->npt[2]) ;
    return res ;
}
//-----------------------------------------------------

#ifdef EMSCRIPTEN
#ifdef USEBINDINGS
    #include "em_bindings.h"
#endif
#endif
