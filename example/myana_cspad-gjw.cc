/* $Id: myana_cspad.cc,v 1.6 2010/10/21 22:09:43 weaver Exp $ */

#include "../myana.hh"
#include "../main.hh"
#include "../XtcRun.hh"
#include "../release/pdsdata/cspad/ConfigV1.hh"
#include "../release/pdsdata/cspad/ConfigV2.hh"
#include "../release/pdsdata/cspad/ElementHeader.hh"
#include "../release/pdsdata/cspad/ElementIterator.hh"
#include "CspadTemp.hh"
#include "CspadCorrector.hh"
#include "CspadGeometry.hh"

#include <stdio.h>
#include <string.h>

using namespace std;

static CspadCorrector*      corrector;

static Pds::CsPad::ConfigV1 configV1;
static Pds::CsPad::ConfigV2 configV2;
static unsigned             configVsn;
static unsigned             quadMask;
static unsigned             asicMask;

static const unsigned  ROWS = 194;
static const unsigned  COLS = 185;

static uint32_t nevents = 0;

class MyQuad {
public:
  MyQuad(unsigned q) : _quad(q) {
    char buff[64];
    for(unsigned i=0; i<16; i++) {
      sprintf(buff,"Q%d_ASIC%d_values",q,i);
//      _h1[i] = new MyTH1(buff,1<<8,6,0);
//      _h1[i] = new MyTH1(buff,1<<9,6,-1000);
      sprintf(buff,"Q%d_ASIC%d_map",q,i);
//      _h2[i] = new MyTH2(buff,COLS,ROWS);
    }
  }
public:
  void Fill(Pds::CsPad::ElementIterator& iter) {
    unsigned section_id;
    const Pds::CsPad::Section* s;
    while((s=iter.next(section_id))) {
//      CspadSection& f = corrector->apply(*s,section_id,_quad);
//      unsigned asic1 = section_id<<1;
//      unsigned asic2 = asic1 + 1;
      for(unsigned col=0; col<COLS; col++)
	for(unsigned row=0; row<ROWS; row++) {	
//	  int v1 = int(f[col][row     ]);
//	  int v2 = int(f[col][row+ROWS]);
//	  _h1[asic1]->Fill(v1);
//	  _h1[asic2]->Fill(v2);
//	  _h2[asic1]->Fill(col,row,v1);
//	  _h2[asic2]->Fill(col,row,v2);
	}
    }
  }    
  void write() {
    for(unsigned i=0; i<16; i++) {
//	_h1[i]->hist();
//	_h2[i]->hist();
    }
  }
//  const CspadSection& section(unsigned section_id)
//  {
//    const int* asic1 = _h2[(section_id<<1)+0]->contents();
//    const int* asic2 = _h2[(section_id<<1)+1]->contents();
//   for(unsigned col=0; col<COLS; col++)
//      for(unsigned row=0; row<ROWS; row++) {	
//	_s[col][row]      = *asic1++;
//	_s[col][row+ROWS] = *asic2++;
//     }
//    return _s;
//  }
private:
  unsigned _quad;
//  MyTH1* _h1[16];
//  MyTH2* _h2[16];
  CspadSection _s;
};

static bool beamOn()
{
  int nfifo = getEvrDataNumber();
  for(int i=0; i<nfifo; i++) {
    unsigned eventCode, fiducial, timestamp;
    if (getEvrData(i,eventCode,fiducial,timestamp)) 
      printf("Failed to fetch evr fifo data\n");
    else if (eventCode==140)
      return true;
  }
  return false;;
}

static MyQuad* quads[4];
    
using namespace Pds;

// This function is called once at the beginning of the analysis job,
// You can ask for detector "configuration" information here.

void beginjob() {
  int fail = 0;
  /*
   * Get time information
   */
  int seconds, nanoSeconds;
  getTime( seconds, nanoSeconds );
    
  const char* time;
  fail = getLocalTime( time );

  corrector = new CspadCorrector(Pds::DetInfo::XppGon,0,
				 CspadCorrector::DarkFrameOffset);
  // 0);
				 
  for(unsigned i=0; i<4; i++)
    quads[i] = new MyQuad(i);

  printf("beginjob\n");
}

// This function is called once for each run.  You should check to see
// if detector configuration information has changed.

void fetchConfig()
{
  if (getCspadConfig( Pds::DetInfo::XppGon, configV1 )==0) {
    configVsn= 1;
    quadMask = configV1.quadMask();
    asicMask = configV1.asicMask();
    printf("CSPAD configuration: quadMask %x  asicMask %x  runDelay %d\n",
	   quadMask,asicMask,configV1.runDelay());
    printf("\tintTime %d/%d/%d/%d\n",
	   configV1.quads()[0].intTime(),
	   configV1.quads()[1].intTime(),
	   configV1.quads()[2].intTime(),
	   configV1.quads()[3].intTime());
  }
  else if (getCspadConfig( Pds::DetInfo::XppGon, configV2 )==0) {
    configVsn= 2;
    quadMask = configV2.quadMask();
    asicMask = configV2.asicMask();
    printf("CSPAD configuration: quadMask %x  asicMask %x  runDelay %d\n",
	   quadMask,asicMask,configV2.runDelay());
    printf("\tintTime %d/%d/%d/%d\n",
	   configV2.quads()[0].intTime(),
	   configV2.quads()[1].intTime(),
	   configV2.quads()[2].intTime(),
	   configV2.quads()[3].intTime());
  }
  else {
    configVsn= 0;
    printf("Failed to get CspadConfig\n");
  }
}

void beginrun() 
{
  fetchConfig();
  printf("beginrun\n");

  corrector->loadConstants(getRunNumber());
}

void begincalib()
{
  fetchConfig();
  printf("begincalib\n");
}

// This is called once every shot.  You can ask for
// the individual detector shot data here.

void event() 
{
  static int ievent = 0;

  int fail = 0;
  /*
   * Get time information
   */
  int seconds, nanoSeconds;
  getTime( seconds, nanoSeconds );
    
  const char* time;
  fail = getLocalTime( time );

  unsigned fiducials;
  fail = getFiducials(fiducials);

  bool beam = beamOn();
//  if (ievent<10)
    printf("Beam %s : fiducial %x\n", beam ? "On":"Off", fiducials);
  
  double gasdet[4];
  if (getFeeGasDet( gasdet )==0 && ievent<10)
    printf("gasdet %g/%g/%g/%g\n",
	   gasdet[0], gasdet[1], gasdet[2], gasdet[3]);
  Pds::CsPad::ElementIterator iter;
  //
  //  Dump some data from the first events
  //
//  if (ievent<10) {
//    if (getCspadData(DetInfo::XppGon, iter)==0) {
//      const Pds::CsPad::ElementHeader* element;
//      while( (element=iter.next()) ) {  // loop over elements (quadrants)
//	printf("Get XppGon Temp { %3.1f %3.1f %3.1f %3.1f }\n",
//	       CspadTemp::instance().getTemp(element->sb_temp(0)),
//	       CspadTemp::instance().getTemp(element->sb_temp(1)),
//	       CspadTemp::instance().getTemp(element->sb_temp(2)),
//	       CspadTemp::instance().getTemp(element->sb_temp(3))
//	);
//	
//	const Pds::CsPad::Section* s;
//	unsigned section_id;
//	while( (s=iter.next(section_id)) ) {  // loop over sections (two by one's)
//	  printf("           Section %d  { %04x %04x %04x %04x }\n",
//		 section_id, s->pixel[0][0], s->pixel[0][1], s->pixel[0][2], s->pixel[0][3]);
//	}
//    }
//    }
//  }
  //
  //  Do some real analysis here
  //
  if ((fail=getCspadData(DetInfo::XppGon, iter))==0) {
    nevents++;
    const Pds::CsPad::ElementHeader* element;
    while( (element=iter.next()) ) {  // loop over elements (quadrants)
//gjw:  only quad 3 has data during commissioning
     if(element->quad()==2){
//gjw:  check that we are not on a new fiducial
        if (fiducials != element->fiducials())
	printf("Fiducials %x/%d:%x\n",fiducials,element->quad(),element->fiducials());
//gjw:  get temp on strong back 2 
	printf("Temperature: %3.1fC\n",CspadTemp::instance().getTemp(element->sb_temp(2)));
//gjw:  read 2x1 "sections"
	const Pds::CsPad::Section* s;
	unsigned section_id;
//gjw:  hack to write all sections to file
	FILE* fp;
	uint16_t data[COLS*ROWS*16];
	char filename[64];
	strcpy(filename,"");
	sprintf(filename,"%x.raw",element->fiducials());
	fp=fopen(filename,"w+");
	while( (s=iter.next(section_id)) ) {  // loop over sections (two by one's)
//	  printf("           Section %d  { %04x %04x %04x %04x }\n",
//		 section_id, s->pixel[0][0], s->pixel[0][1], s->pixel[0][2], s->pixel[0][3]);
//gjw:  read out data in DAQ format, i.e., 2x8 array of asics (two bytes / pixel)
		memcpy(&data[section_id*2*ROWS*COLS],s->pixel[0],2*2*ROWS*COLS);
	}
//gjw:  write out 2x8 array as binary 16 bit file
	fwrite(data,sizeof(char)*2,ROWS*COLS*16,fp);
	fclose(fp);
//gjw:  split 2x8 array into 2x2 (detector unit for rotation), 4 in a quad and write to file
	uint16_t tbt[4][2*COLS][2*ROWS];
	for(int g=0;g<4;g++){
		memcpy(&tbt[g],&data[g*2*2*ROWS*COLS],2*4*COLS*ROWS);
		strcpy(filename,"");
		sprintf(filename,"%x-q%d.raw",element->fiducials(),g);
		FILE* fp1=fopen(filename,"w+");
		fwrite(&tbt[g][0][0],sizeof(char)*2,ROWS*COLS*4,fp1);
		fclose(fp1);
	}
//gjw:  rotate/reflect 2x2s into consistent orientations
	uint16_t buff[2*COLS][2*ROWS];
	uint16_t buff1[2*ROWS][2*COLS];
	uint16_t buff2[2*COLS][2*ROWS];
	uint16_t buff3[2*ROWS][2*COLS];
	FILE* fp1;
//quad 0 (asics 0-3) 370x388 pixels
//reflect
	for(unsigned int g=0;g<2*COLS;g++)for(unsigned int j=0;j<2*ROWS;j++)
	 buff[g][j]=tbt[0][2*COLS-1-g][j];	
//rotate 90
	for(unsigned int g=0;g<2*ROWS;g++)for(unsigned int j = 0;j<2*COLS;j++){
		buff1[g][j]=buff[2*ROWS-1-j+2*(COLS-ROWS)][g];
	}
	strcpy(filename,"");
	sprintf(filename,"%x-q%d-corrected.raw",element->fiducials(),0);
	fp1=fopen(filename,"w+");
	fwrite(buff1[0],sizeof(char)*2,ROWS*COLS*4,fp1);
	fclose(fp1);
//quad 1 (asics 4-7) 388x370
//reflect	
	for(unsigned int g=0;g<2*COLS;g++)for(unsigned int j=0;j<2*ROWS;j++)
	 buff[g][j]=tbt[1][2*COLS-1-g][j];	
//reflect	
	for(unsigned int g=0;g<2*COLS;g++)for(unsigned int j=0;j<2*ROWS;j++)
	 buff3[g][j]=buff[g][2*ROWS-1-j];	
	strcpy(filename,"");
	sprintf(filename,"%x-q%d-corrected.raw",element->fiducials(),1);
	fp1=fopen(filename,"w+");
	fwrite(buff3[0],sizeof(char)*2,ROWS*COLS*4,fp1);
	fclose(fp1);

//quad 2 (asics 8-11) 370x388
//rotate -90
	for(unsigned int g=0;g<2*ROWS;g++)for(unsigned int j = 0;j<2*COLS;j++){
		buff1[g][j]=tbt[2][j][2*COLS-1-g+2*(ROWS-COLS)];
	}
	strcpy(filename,"");
	sprintf(filename,"%x-q%d-corrected.raw",element->fiducials(),2);
	fp1=fopen(filename,"w+");
	fwrite(buff1[0],sizeof(char)*2,ROWS*COLS*4,fp1);
	fclose(fp1);

//quad 3 (asics 12-15) 388x370
//reflect	
	for(unsigned int g=0;g<2*COLS;g++)for(unsigned int j=0;j<2*ROWS;j++)
	 buff[g][j]=tbt[3][2*COLS-1-g][j];	
//reflect	
	for(unsigned int g=0;g<2*COLS;g++)for(unsigned int j=0;j<2*ROWS;j++)
	 buff2[g][j]=buff[g][2*ROWS-1-j];	
	strcpy(filename,"");
	sprintf(filename,"%x-q%d-corrected.raw",element->fiducials(),3);
	fp1=fopen(filename,"w+");
	fwrite(buff2[0],sizeof(char)*2,ROWS*COLS*4,fp1);
	fclose(fp1);
     }
   }
  }
  else
    printf("getCspadData fail %d (%x)\n",fail,fiducials);

  ++ievent;
}

void endcalib()
{
}

void endrun() 
{
  printf("User analysis endrun() routine called.\n");
}

void endjob()
{
  printf("User analysis endjob() routine called.\n");

  for(unsigned i=0; i<4; i++)
    quads[i]->write();

  CspadGeometry geom;
  const unsigned nb = 3400;
  double* array = new double[(nb+2)*(nb+2)];
  double sz = 0.25*double(nb)*109.92;
  CsVector offset; offset[0]=sz; offset[1]=sz;

    for(unsigned i=0; i<4; i++) {
//  { unsigned i=2;
//   for(unsigned s=0; s<8; s++) {
//      const CspadSection&    sect = quads[i]->section(s);
//      const SectionGeometry& sg   = geom.section(i,s); 
//      sg.map(sect, array+nb+3, 0.5*109.92, nb+2, offset);
//    }
//  }

//  TH2F* hgeom = new TH2F("geo","geo",nb,-sz,sz,nb,-sz,sz);
//  hgeom->SetContent(array);

  memset(array,0,(nb+2)*(nb+2)*sizeof(double));
//  CspadSection sect;
//  { float* p = &sect[0][0];
//    float* e = p + sizeof(CspadSection)/sizeof(float);
//    while( p < e ) *p++ = 1.; }
//  { unsigned i=2;
//   for(unsigned s=0; s<8; s++) {
//      const SectionGeometry& sg   = geom.section(i,s); 
//      sg.map(sect, array+nb+3, 0.5*109.92, nb+2, offset);
//    }
//  }

//  TH2F* hnorm = new TH2F("nrm","nrm",nb,-sz,sz,nb,-sz,sz);
//  hnorm->SetContent(array);

//  hgeom->Divide(hgeom,hnorm,1.,double(nevents));
}
}
