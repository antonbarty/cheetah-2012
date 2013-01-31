#ifndef PSANA_EXAMPLES_DUMPIPIMB_H
#define PSANA_EXAMPLES_DUMPIPIMB_H

//--------------------------------------------------------------------------
// File and Version Information:
// 	$Id: DumpIpimb.h 1919 2011-05-21 12:04:08Z salnikov@SLAC.STANFORD.EDU $
//
// Description:
//	Class DumpIpimb.
//
//------------------------------------------------------------------------

//-----------------
// C/C++ Headers --
//-----------------

//----------------------
// Base Class Headers --
//----------------------
#include "psana/Module.h"

//-------------------------------
// Collaborating Class Headers --
//-------------------------------

//------------------------------------
// Collaborating Class Declarations --
//------------------------------------

//		---------------------
// 		-- Class Interface --
//		---------------------

namespace psana_examples {

/**
 *  @brief Example module class for psana
 *
 *  This software was developed for the LCLS project.  If you use all or 
 *  part of it, please give an appropriate acknowledgment.
 *
 *  @see AdditionalClass
 *
 *  @version $Id: DumpIpimb.h 1919 2011-05-21 12:04:08Z salnikov@SLAC.STANFORD.EDU $
 *
 *  @author Andrei Salnikov
 */

class DumpIpimb : public Module {
public:

  // Default constructor
  DumpIpimb (const std::string& name) ;

  // Destructor
  virtual ~DumpIpimb () ;

  /// Method which is called at the beginning of the calibration cycle
  virtual void beginCalibCycle(Event& evt, Env& env);
  
  /// Method which is called with event data
  virtual void event(Event& evt, Env& env);

protected:

private:

  Source m_src;

};

} // namespace psana_examples

#endif // PSANA_EXAMPLES_DUMPIPIMB_H