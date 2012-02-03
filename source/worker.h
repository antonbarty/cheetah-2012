/*
 *  worker.h
 *  cheetah
 *
 *  Created by Anton Barty on 6/2/11.
 *  Copyright 2011 CFEL. All rights reserved.
 *
 */






/*
 *	Structure used for passing information to worker threads
 */
typedef struct {
	
	// Reference to common global structure
	cGlobal		*pGlobal;
	int			busy;
	long		threadNum;
	
	// cspad data
	int			cspad_fail;
	float		quad_temperature[4];
	uint16_t	*quad_data[4];
	uint16_t	*raw_data;
	float		*corrected_data;
	float		*detector_corrected_data;
	int16_t		*corrected_data_int16;
	int16_t		*image;
	float		*radialAverage;
	float		*radialAverageCounter;
	int			nPeaks;
	int			nHot;
	int16_t *   saturatedPixelMask;
	
	
	// TOF data
	int			TOFPresent;
	double		*TOFTime;
	double		*TOFVoltage;
	double		TOFtrigtime ;
	
	// Peak info
	long		*peak_com_index;		// closest pixel corresponding to peak center of mass
	float		*peak_com_x;			// peak center of mass x (in raw layout)
	float		*peak_com_y;			// peak center of mass y (in raw layout)
	float		*peak_com_x_assembled;	// peak center of mass x (in assembled layout)
	float		*peak_com_y_assembled;	// peak center of mass y (in assembled layout)
	float		*peak_com_r_assembled;	// peak center of mass r (in assembled layout)
	float		*peak_intensity;		// integrated peak intensities
	float		*peak_npix;				// Number of pixels in peak
	float       *peak_snr;           // Signal-to-noise of peak
	float		peakResolution;			// Radius of 80% of peaks
	float		peakDensity;			// Density of peaks within this 80% figure
	float		peakNpix;				// Number of pixels in peaks
	float		peakTotal;				// Total integrated intensity in peaks
	int			*good_peaks;           // Good peaks, after post peak-finding criteria	
	
	// Beamline data, etc
	int			seconds;
	int			nanoSeconds;
	unsigned	fiducial;
	char		timeString[1024];
	char		eventname[1024];
	bool		beamOn;
	unsigned	runNumber;
	bool		laserEventCodeOn;

	double      gmd1;
    double      gmd2;
	double		gmd11;
	double		gmd12;
	double		gmd21;
	double		gmd22;
    
    double      laserDelay;
	
	double		fEbeamCharge;		// in nC
	double		fEbeamL3Energy;		// in MeV
	double		fEbeamLTUPosX;		// in mm
	double		fEbeamLTUPosY;		// in mm
	double		fEbeamLTUAngX;		// in mrad
	double		fEbeamLTUAngY;		// in mrad
	double		fEbeamPkCurrBC2;	// in Amps
	
	double		photonEnergyeV;		// in eV
	double		wavelengthA;		// in Angstrom
	
	double		detectorPosition; 
	
	double		phaseCavityTime1;
	double		phaseCavityTime2;
	double		phaseCavityCharge1;
	double		phaseCavityCharge2;
	
	// Thread management
	int		threadID;
	
	
	
} tThreadInfo;




static uint32_t nevents = 0;

#define DEBUGL1_ONLY if(global->debugLevel >= 1)
#define DEBUGL2_ONLY if(global->debugLevel >= 2)

/*
 *	Function prototypes
 */
void *worker(void *);
void assemble2Dimage(tThreadInfo*, cGlobal*);
//void calculateRadialAverage(tThreadInfo*, cGlobal*);
void checkSaturatedPixels(tThreadInfo *threadInfo, cGlobal *global);

// detectorCorrection.cpp
void subtractDarkcal(tThreadInfo*, cGlobal*);
void cmModuleSubtract(tThreadInfo*, cGlobal*);
void cmSubtractUnbondedPixels(tThreadInfo*, cGlobal*);
void cmSubtractBehindWires(tThreadInfo*, cGlobal*);
void applyGainCorrection(tThreadInfo*, cGlobal*);
void applyBadPixelMask(tThreadInfo*, cGlobal*);
void calculateHotPixelMask(cGlobal*);
void killHotpixels(tThreadInfo*, cGlobal*);


// backgroundCorrection.cpp
void updateBackgroundBuffer(tThreadInfo*, cGlobal*);
void calculatePersistentBackground(cGlobal*);
void subtractPersistentBackground(tThreadInfo*, cGlobal*);
void subtractLocalBackground(tThreadInfo*, cGlobal*);

// saveFrame.cpp
void nameEvent(tThreadInfo*, cGlobal*);
void writeHDF5(tThreadInfo*, cGlobal*);
void writePeakFile(tThreadInfo *threadInfo, cGlobal *global);
void writeSimpleHDF5(const char*, const void*, int, int, int);


// hitfinders.cpp
int  hitfinder(tThreadInfo*, cGlobal*);

// powder.cpp
void addToPowder(tThreadInfo*, cGlobal*, int);
void writePowderData(char*, void*, int, int, void*, void*, long, long, int);
void saveRunningSums(cGlobal*);

// RadialAverage.cpp
void addToRadialAverageStack(tThreadInfo*, cGlobal*, int);
void saveRadialAverageStack(cGlobal*, int);
void saveRadialStacks(cGlobal*);

void calculateRadialAverage(float*, float*, float*, cGlobal*);
void calculateRadialAverage(double*, double*, double*, cGlobal*);

// median.cpp
int16_t kth_smallest(int16_t*, long, long);

// fudge...
void evr41fudge(tThreadInfo *t, cGlobal *g);



