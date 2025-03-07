/** \addtogroup DEM Discrete Element Simulations
 * This module handles the Discrete Element Simulations.
 *  @{ */
#include <vector>
#include <list>
#include "Typedefs.h"
#include "Parameters.h"
#include "Tools.h"


#ifndef CONTACTLIST
#define CONTACTLIST

// ------------------------------------ Action class -------------------------------------------
/** \brief Handle force and torque contact information
 * */
class Action {
public :
    vector <double> Fn, Ft, Torquei, Torquej ;
    vector <double> vn, vt ;
    void set (v1d a, v1d b, v1d c, v1d d) {Fn=a ; Ft=b ; Torquei=c ; Torquej=d ; }
    void setvel(v1d vvn, v1d vvt) {vn=vvn; vt=vvt ;}
    void setzero (int d) {Fn=(v1d(d,0)) ; Ft=(v1d(d,0)) ; vn=(v1d(d,0)) ; vt=(v1d(d,0)) ; Torquei=(v1d(d*(d-1)/2,0)) ; Torquej=(v1d(d*(d-1)/2)) ; }
    //void set_fwd (v1d a, v1d b, v1d c) {Fni=a; Fti=b; Torquei=c ;}
    //  void set_rev (v1d a, v1d b, v1d c) {Fnj=a; Ftj=b; Torquej=c ;}
} ;
/** \brief Action on a specific particle for a specific duration
 * */
class SpecificAction: public Action {
public :
    int id ;
    int duration ;
} ;

// ------------------------------------ Contact properties class -------------------------------------------
/** \brief Contact properties class.
 *  \par How ghost work
 *  Image particles though periodic boundary conditions (PBC) are called ghost. A ghost can be an image through multiple PBC.
 *  #ghost indicate if particle #j is making contact through 1 or more PBC (of #ghost>0), and if so through which PBC. A bit set to 1 at position n in #ghost indicates an image through the PBC in the n-th dimension. Least significant bit (LSB) correspond to dimension 0. Walls are counted identically as PBC for dimension counting.
 *  #ghostdir follows the same logic as #ghost but a bit set to 1 (resp. 0) at position n indicates an image through the high (resp. low) n-th dimension, which should be a PBC.
 *
 *  \warning A #ghost bit of 1 should never happen on a non PBC dimension. A #ghostdir bit of 1 should never happen on a non PBC dimension, or without a 1 bit at the same location in #ghost.
 *  \warning The number of dimensions is limited to sizeof(uint32_t) because of these ghost, as they are using 32 bit datatype. There is a risk of overflowing the datatype if using PBC at higher than 32 dimensions, or if using a PBC in dimension 32 (since the last bit could be interpreted as a sign bit). Walls in dimensions higher than 32 may be ok, but without guarantee.
 *  \todo Ghosts should be improved in order to use either a bitset datatype, or a custom datatype, to avoid the limitation to sizeof(uint32_t) in the number of dimension.
 */
class cp
{
public:
 cp (int ii, int jj, int d, double ctlength, Action * default_action) : i(ii), j(jj), contactlength(ctlength), tspr (vector <double> (d, 0)), infos(default_action), owninfos(false){} ///< New contact creation
 ~cp () { if (owninfos) delete(infos) ; } ///< Remove & clean contact
 cp & operator= (const cp & c)
 {
     i=c.i ;
     if (c.i<0) return *this ; //is a deleted element, just keep moving
     j=c.j ; // copy everything else if it is not a deleted element
     ghost=c.ghost ;
     ghostdir=c.ghostdir ;
     tspr=c.tspr ;
     contactlength=c.contactlength ;
     infos=c.infos ;
     return *this ;
 } ///< Affect contact.

 Action & getinfo () {return *infos ; } ///< Returning stored information \warning Poorly tested
 //void setinfo (Action & a) {if (!infos) infos = new Action ; *infos=a ; }
 void setinfo (Action * a) {infos=a ; } ///< Set information for contact force.
 void saveinfo (Action & a) {
     if (!owninfos)
     {
         infos = new Action ;
         owninfos = true ;
     }
     infos->Fn = a.Fn ;
     //printf("%g %g %g\n",a.Fn[0], a.Fn[1], a.Fn[2]) ;
     infos->Ft = a.Ft ;
     infos->vn = a.vn ;
     infos->vt = a.vt ;
     infos->Torquei = a.Torquei ;
     infos->Torquej = a.Torquej ;
 } ///< Save information regarding the contact forces for later write down.

 int i ;  ///< Index of contacting particle.
 int j ; ///< Index of second contacting particle or wall. If this is a wall contact, j=2*walldimension + (0 or 1 if it is the low wall or high wall)
 double contactlength ; ///< Length of the contact
 uint32_t ghost ; ///< Contain ghost information about ghost contact, cf detailed description
 uint32_t ghostdir ; ///< Contain ghost information about ghost direction, cf detailed description
 vector <double> tspr ; ///< Vector of tangential contact history
 Action * infos ; ///< stores contact information if contact storing is requires \warning Poorly tested.
 bool owninfos ; ///< True if the contact contains stored information for dump retrieval
} ;

// ------------------------------------ Contact List class ----------check_ghost_LE---------------------------------
/** \brief Handles lists of contacts
 */
template <int d>
class ContactList
{
public:
 ContactList () {check_ghost=&ContactList::check_ghost_LE;}
 void reset() {it = v.begin() ;}  ///< Go to the contact list beginning
 int insert(const cp& a) ; ///< Insert a contact, maintaining sorting with increasing i, and removing missing contacts on traversal.
 void finalise () { while (it!=v.end()) it=v.erase(it) ; } ///< Go to the end of the contact list, erasing any remaining contact which open.
 list <cp> v ; ///< Contains the list of contact
 Action * default_action () {return (&def) ; } ///< Easy allocation of a default contact to initialise new contacts.
 int cid=0 ; ///< \deprecated not used for anything anymore I think.

 //void check_ghost    (uint32_t gst, double partialsum, const Parameters & P, cv1d &X1, cv1d &X2, double R, cp & tmpcp) ;
 void check_ghost_dst(uint32_t gst, int n, double partialsum, uint32_t mask, const Parameters<d> & P, cv1d &X1, cv1d &X2, cp & contact) ; ///< \deprecated Measure distance between a ghost and a particle
 void check_ghost_regular (bitdim gst, const Parameters<d> & P, cv1d &X1, cv1d &X2, cp & tmpcp,
                   int startd=0, double partialsum=0, bitdim mask=0) ; ///< Find ghost-particle contact, going though pbc recursively. A beautiful piece of optimised algorithm if I may say so myself.
 void check_ghost_LE (bitdim gst, const Parameters<d> & P, cv1d &X1, cv1d &X2, cp & tmpcp, int startd=0, double partialsum=0, bitdim mask=0) ;
 void (ContactList::*check_ghost) (bitdim , const Parameters<d> & , cv1d &, cv1d &, cp &,int startd, double partialsum, bitdim mask) ;
 void coordinance (v1d &Z) ; ///< Calculate and store coordination number in Z.

private:
 list<cp>::iterator it ; ///< Iterator to the list to allow easy traversal, insertion & deletion while maintening ordering.
 Action def ; ///< Default action
};

inline bool operator< (const cp &a, const cp &b) {if (a.i==b.i) return (a.j<b.j) ; return a.i<b.i ; } ///< The contact list is order in increasing order of index i, and for two identical i in increasing order of j.
inline bool operator== (const cp &a, const cp &b) {return (a.i==b.i && a.j==b.j) ; } ///< Contact equivalence is based solely on the index of objects in contact i and j.


/*****************************************************************************************************
 *                                                                                                   *
 *                                                                                                   *
 *                                                                                                   *
 * IMPLEMENTATIONS                                                                                   *
 *                                                                                                   *
 *                                                                                                   *
 *                                                                                                   *
 * ***************************************************************************************************/

template <int d>
int ContactList<d>::insert(const cp &a)
{
    while (it!=v.end() && (*it) < a)
    { it= v.erase(it) ; }
    if (it==v.end()) {v.push_back(a) ; it=v.end() ; }
    else
    {
        if ((*it)==a)
        {
            it->contactlength=a.contactlength ;
            it->ghost=a.ghost ;
            it->ghostdir=a.ghostdir ;
            it++ ;
        }
        else {it=v.insert(it,a) ; it++ ; }
    }
    return (cid++) ;
}

//-----------------------------------Fastest version so far ...
template <int d>
void ContactList<d>::check_ghost_regular (bitdim gst, const Parameters<d> & P, cv1d &X1, cv1d &X2, cp & tmpcp,
                                 int startd, double partialsum, bitdim mask)
{
    double sum=partialsum ;
    for (int dd=startd ; sum<P.skinsqr && dd<d ; dd++, gst>>=1)
    {
        sum += (X1[dd]-X2[dd]) * (X1[dd]-X2[dd]) ;
        if (gst & 1)
        {
            double Delta= (tmpcp.ghostdir&(1<<dd)?-1:1) * P.Boundaries[dd][2] ;
            double sumspawn = partialsum + (X1[dd]-X2[dd]-Delta) * (X1[dd]-X2[dd]-Delta) ;
            //printf("/%g %g %g %g %g %g %g/", partialsum, sumspawn, X1[0], X1[1], X2[0], X2[1], Delta ) ;
            if (sumspawn<P.skinsqr)
                check_ghost_regular (gst>>1, P, X1, X2, tmpcp, dd+1, sumspawn, mask | (1<<dd)) ;
        }
        partialsum = sum ;
    }
    if (sum<P.skinsqr)
    {
        tmpcp.contactlength=sqrt(sum) ;
        tmpcp.ghost=mask ;
        insert(tmpcp) ;
        //printf("[%d %d %X %X %g]\n", tmpcp.i, tmpcp.j, tmpcp.ghost, tmpcp.ghostdir, tmpcp.contactlength) ;
    }
}

//--------------------------------------------------------------------
template <int d>
void ContactList<d>::check_ghost_LE (bitdim gst, const Parameters<d> & P, cv1d &X1, cv1d &X2, cp & tmpcp,
                                     [[maybe_unused]] int startd, [[maybe_unused]]double partialsum, [[maybe_unused]]bitdim mask)
{
    if (P.Boundaries[0][3] != static_cast<int>(WallType::PBC_LE))
            check_ghost_regular(gst, P, X1, X2, tmpcp) ;
    else
    {
     if ((gst & 1)==0)
     {
         double partialsum = (X1[0]-X2[0]) * (X1[0]-X2[0]) ;
         assert (((gst>>30 & 1) ==0)) ;
         check_ghost_regular(gst>>1, P, X1, X2, tmpcp, 1, partialsum, 0) ;
     }
     else //There is an image through the LE
     {
         //1 : case without taking that image
         double partialsum = (X1[0]-X2[0]) * (X1[0]-X2[0]) ;
         bitdim newgst = gst ; newgst &= (~(1<<30)) ;
         check_ghost_regular(newgst>>1, P, X1, X2, tmpcp, 1, partialsum, 0) ;

         //2: now is the hard case: we take the image path
         double Delta= (tmpcp.ghostdir&1?-1:1) * P.Boundaries[0][2] ;
         partialsum = (X1[0]-X2[0]-Delta) * (X1[0]-X2[0]-Delta) ;
         newgst = gst ;
         newgst &= (~(1<<1)) ;      // Clearing bit 1
         newgst |= ((gst>>30)<<1) ; // Setting bit 1 to the value of bit 30
         newgst &= (~(1<<30)) ;     // Clearing bit 30
         tmpcp.ghostdir &= (~(1<<1)) ; //Clearing bit 1
         tmpcp.ghostdir |= ((tmpcp.ghostdir>>30)<<1) ; // Setting the value of bit 1 to the value of bit 30. No need for clearing in the ghostdir.
         auto tmpX2 = X2 ;
         tmpX2[1] += (tmpcp.ghostdir&1?-1:1)*P.Boundaries[0][5] ;
         if (tmpX2[1] > P.Boundaries[1][1]) tmpX2[1] -= P.Boundaries[1][2] ;
         if (tmpX2[1] < P.Boundaries[1][0]) tmpX2[1] += P.Boundaries[1][2] ;
         //printf("{%g %g %X %X", tmpX2[0], tmpX2[1], newgst>>1, tmpcp.ghostdir) ;
         check_ghost_regular(newgst>>1, P, X1, tmpX2, tmpcp, 1, partialsum, 1) ;
         //printf("}") ;
     }
    }
}

//----------------------------------------
template <int d>
void ContactList<d>::check_ghost_dst(uint32_t gst, int n, double partialsum, uint32_t mask, const Parameters<d> & P, cv1d &X1, cv1d &X2, cp & contact)
{
  if (gst==0) return ;
  else
  {
    for ( ;(gst&1)==0; gst>>=1,n++) ;
    check_ghost_dst(gst-1, n, partialsum, mask, P, X1, X2, contact) ;
    double Delta= (contact.ghostdir&(1<<n)?-1:1) * P.Boundaries[n][2] ;
    partialsum = partialsum + Delta*(2*(X2[n]-X1[n]) + Delta) ;
    if (partialsum<contact.contactlength) // Found a lower distance with this ghost
    {
      contact.contactlength=partialsum ;
      contact.ghost = mask|(1<<n) ;
    }
    check_ghost_dst(gst-1, n, partialsum, mask|(1<<n), P, X1, X2, contact) ;
  }
}

//-----------------------------------
template <int d>
void ContactList<d>::coordinance (v1d &Z)
{
  for (auto & w : v)
  {
      Z[w.i]++ ; Z[w.j] ++ ;
  }
}

#endif
/** @} */
