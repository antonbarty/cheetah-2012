
#include "myana/myana.hh"
#include "myana/main.hh"
#include "myana/XtcRun.hh"
#include "release/pdsdata/cspad/ConfigV1.hh"
#include "release/pdsdata/cspad/ConfigV2.hh"
#include "release/pdsdata/cspad/ConfigV3.hh"
#include "release/pdsdata/cspad/ElementHeader.hh"
#include "release/pdsdata/cspad/ElementIterator.hh"
#include "cspad/CspadTemp.hh"
#include "cspad/CspadCorrector.hh"
#include "cspad/CspadGeometry.hh"

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <hdf5.h>
#include <stdlib.h>

#include "setup.h"
#include "worker.h"
#include "median.h"
#include "hitfinders.h"


// Peakfinders local to this routine
int peakfinder3(cGlobal *global, tThreadInfo *threadInfo);
int peakfinder5(cGlobal *global, tThreadInfo *threadInfo);
int peakfinder6(cGlobal *global, tThreadInfo *threadInfo);



/*
 *	Various flavours of hitfinder
 *		1 - Number of pixels above ADC threshold
 *		2 - Total intensity above ADC threshold
 *		3 - Count Bragg peaks
 *		4 - Use TOF
 *		5 - Like 3, but with extras
 *		6 - Experimental - find peaks by SNR criteria
 */
int  hitfinder(tThreadInfo *threadInfo, cGlobal *global){
	
	long	nat;
	long	counter;
	int		hit=0;
	float	total;
	long *inx = (long *) calloc(global->pix_nn, sizeof(long));
	long *iny = (long *) calloc(global->pix_nn, sizeof(long));
	float mingrad = global->hitfinderMinGradient*2;
	mingrad *= mingrad;
	
	nat = 0;
	counter = 0;
	total = 0.0;
	
	/*
	 *	Default values for some metrics
	 */
	threadInfo->peakNpix = 0;
	threadInfo->peakTotal = 0;
	threadInfo->peakResolution = 0;
	threadInfo->peakDensity = 0;
	
	
	/*
	 *	Use a data buffer so we can zero out pixels already counted
	 */
	float *temp = (float*) calloc(global->pix_nn, sizeof(float));
	memcpy(temp, threadInfo->corrected_data, global->pix_nn*sizeof(float));
	
	/*
	 *	Apply peak search mask 
	 *	(multiply data by 0 to ignore regions)
	 */
	if(global->hitfinderUsePeakmask) {
		for(long i=0;i<global->pix_nn;i++){
			temp[i] *= global->peakmask[i]; 
		}
	}
	
	
	// Combined mask
	int* mask = (int *) calloc(global->pix_nn, sizeof(int) );
	memcpy(mask,global->hitfinderResMask,global->pix_nn*sizeof(int));
	for (long i=0; i<global->pix_nn; i++) 
        mask[i] *= global->hotpixelmask[i] * global->badpixelmask[i] * threadInfo->saturatedPixelMask[i];
	
    
	/*
	 *	Use one of various hitfinder algorithms
	 */
	switch(global->hitfinderAlgorithm) {
			
		case 1 :	// Count the number of pixels above ADC threshold
			for(long i=0;i<global->pix_nn;i++){
				if(temp[i] > global->hitfinderADC){
					total += temp[i];
					nat++;
				}
			}
			if(nat >= global->hitfinderNAT)
				hit = 1;
			
			threadInfo->peakNpix = nat;
			threadInfo->nPeaks = nat;
			threadInfo->peakTotal = total;
			break;
			
			
		case 2 :	//	integrated intensity above threshold
			for(long i=0;i<global->pix_nn;i++){
				if(temp[i] > global->hitfinderADC){
					total += temp[i];
					nat++;
				}
			}
			if(total >= global->hitfinderTAT) 
				hit = 1;
			
			threadInfo->peakNpix = nat;
			threadInfo->nPeaks = nat;
			threadInfo->peakTotal = total;
			break;
			
		case 3 : 	// Count number of Bragg peaks
			hit = peakfinder3(global, threadInfo);			
			break;	

            
		case 4 :	// Use TOF signal to find hits
			if ((global->hitfinderUseTOF==1) && (threadInfo->TOFPresent==1)){
				double total_tof = 0.;
				for(int i=global->hitfinderTOFMinSample; i<global->hitfinderTOFMaxSample; i++){
					total_tof += threadInfo->TOFVoltage[i];
				}
				if (total_tof > global->hitfinderTOFThresh)
					hit = 1;
			}
			// Use cspad threshold if TOF is not present 
			else {
				for(long i=0;i<global->pix_nn;i++){
					if(temp[i] > global->hitfinderADC){
						nat++;
					}
				}
				if(nat >= global->hitfinderNAT)
					hit = 1;
			}
			break;
			
		case 5 : 	// Count number of Bragg peaks
			hit = peakfinder5(global,threadInfo);
			break;
			
		case 6 : 	// Count number of Bragg peaks
			hit = peakfinder6(global,threadInfo);
			break;
            
		case 7 : 	// Return laser on event code
            hit = threadInfo->laserEventCodeOn;
            threadInfo->nPeaks = threadInfo->laserEventCodeOn;
			break;
			
		default :
			printf("Unknown hit finding algorithm selected: %i\n", global->hitfinderAlgorithm);
			printf("Stopping in confusion.\n");
			exit(1);
			break;
			
	}
	
	// Statistics on the peaks, for certain hitfinders
	if( threadInfo->nPeaks > 1 &&
	   ( global->hitfinderAlgorithm == 3 || global->hitfinderAlgorithm == 5 || global->hitfinderAlgorithm == 6 ) ) {
		   
		   long	np;
		   long  kk;
		   float	resolution;
		   float	cutoff = 0.8;
		   
		   np = threadInfo->nPeaks;
		   if(np >= global->hitfinderNpeaksMax) 
			   np = global->hitfinderNpeaksMax; 
		   
		   float *buffer1 = (float*) calloc(global->hitfinderNpeaksMax, sizeof(float));
		   for(long k=0; k<np; k++) {
			   buffer1[k] = threadInfo->peak_com_r_assembled[k];
		   }
		   kk = (long) floor(cutoff*np);
		   resolution = kth_smallest(buffer1, np, kk);
		   
		   threadInfo->peakResolution = resolution;
		   if(resolution > 0) {
			   float	area = (3.141*resolution*resolution)/(global->asic_ny*global->asic_nx);
			   threadInfo->peakDensity = (cutoff*np)/area;
		   }
		   
		   free(buffer1);
    } 
	
	// Update central hit counter
	if(hit) {
		pthread_mutex_lock(&global->nhits_mutex);
		global->nhits++;
		global->nrecenthits++;
		pthread_mutex_unlock(&global->nhits_mutex);
	}
	

	free(inx); 			
	free(iny);	
	free(mask);	
	free(temp);
	
	return(hit);
	
}



/*
 *	Peakfinder 3
 *	Search of connected pixels above threshold
 *	Anton Barty
 */
int peakfinder3(cGlobal *global, tThreadInfo	*threadInfo) {
	
	long	nat, lastnat;
	long	counter;
	int		hit=0;
	float	total;
	int search_x[] = {-1,0,1,-1,1,-1,0,1};
	int search_y[] = {-1,-1,-1,0,0,1,1,1};
	int	search_n = 8;
	long e;
	long *inx = (long *) calloc(global->pix_nn, sizeof(long));
	long *iny = (long *) calloc(global->pix_nn, sizeof(long));
	float totI;
	float peak_com_x;
	float peak_com_y;
	long thisx;
	long thisy;
	long fs, ss;
	float grad;
	//float lbg, imbg; /* local background nearby peak */
	//float *lbg_buffer;
	//int fsmin, fsmax, ssmin, ssmax;
	//int lbg_ss, lbg_fs, lbg_e;
	//int thisfs, thisss;
	float mingrad = global->hitfinderMinGradient*2;
	mingrad *= mingrad;
	double dx1, dx2, dy1, dy2;
	double dxs, dys;
	
	nat = 0;
	counter = 0;
	total = 0.0;
	
	// Combined mask of all areas to ignore
	int *mask = (int *) calloc(global->pix_nn, sizeof(int) );
	memcpy(mask,global->hitfinderResMask,global->pix_nn*sizeof(int));
	for (long i=0; i<global->pix_nn; i++) 
        mask[i] *= global->hotpixelmask[i] * global->badpixelmask[i] * threadInfo->saturatedPixelMask[i] * global->peakmask[i];
	
	// zero out bad pixels in temporary intensity map
	float* temp = (float *) calloc(global->pix_nn,sizeof(float));
	for (long i=0; i<global->pix_nn; i++) 
        temp[i] = threadInfo->corrected_data[i]*mask[i];
	
    
	// Loop over modules (8x8 array)
	for(long mj=0; mj<global->nasics_y; mj++){
		for(long mi=0; mi<global->nasics_x; mi++){
			
			// Loop over pixels within a module
			for(long j=1; j<global->asic_ny-1; j++){
				for(long i=1; i<global->asic_nx-1; i++){
					
					
					ss = (j+mj*global->asic_ny)*global->pix_nx;
					fs = i+mi*global->asic_nx;
					e = ss + fs;
					
					//if(e >= global->pix_nn)
					//	printf("Array bounds error: e=%i\n");
					
					if(temp[e] > global->hitfinderADC){
						
                        // Rick's gradient test (if minGradient>0)
                        /*
						if ( global->hitfinderMinGradient > 0 ){
							
							// Get gradients 
							dx1 = temp[e] - temp[e+1];
							dx2 = temp[e-1] - temp[e];
							dy1 = temp[e] - temp[e+global->pix_nx];
							dy2 = temp[e-global->pix_nx] - temp[e];
							
							// Average gradient measurements from both sides 
							dxs = ((dx1*dx1) + (dx2*dx2)) / 2;
							dys = ((dy1*dy1) + (dy2*dy2)) / 2;
							
							// Calculate overall gradient 
							grad = dxs + dys;
							
                            if ( grad < global->hitfinderMinGradient ) 
                               continue;
						}
                        */	
						
						
						// This might be the start of a peak - start searching
						inx[0] = i;
						iny[0] = j;
						nat = 1;
						totI = 0; 
						peak_com_x = 0; 
						peak_com_y = 0; 
						
						// Keep looping until the pixel count within this peak does not change
						do {
							
							lastnat = nat;
							// Loop through points known to be within this peak
							for(long p=0; p<nat; p++){
								// Loop through search pattern
								for(long k=0; k<search_n; k++){
									// Array bounds check
									if((inx[p]+search_x[k]) < 0)
										continue;
									if((inx[p]+search_x[k]) >= global->asic_nx)
										continue;
									if((iny[p]+search_y[k]) < 0)
										continue;
									if((iny[p]+search_y[k]) >= global->asic_ny)
										continue;
									
									// Neighbour point 
									thisx = inx[p]+search_x[k]+mi*global->asic_nx;
									thisy = iny[p]+search_y[k]+mj*global->asic_ny;
									e = thisx + thisy*global->pix_nx;
									
									//if(e < 0 || e >= global->pix_nn){
									//	printf("Array bounds error: e=%i\n",e);
									//	continue;
									//}
									
									// Above threshold?
									if(temp[e] > global->hitfinderADC){
										//if(nat < 0 || nat >= global->pix_nn) {
										//	printf("Array bounds error: nat=%i\n",nat);
										//	break
										//}
										totI += temp[e]; // add to integrated intensity
										peak_com_x += temp[e]*( (float) thisx ); // for center of mass x
										peak_com_y += temp[e]*( (float) thisy ); // for center of mass y
										temp[e] = 0; // zero out this intensity so that we don't count it again
										inx[nat] = inx[p]+search_x[k];
										iny[nat] = iny[p]+search_y[k];
										nat++;
									}
								}
							}
						} while(lastnat != nat);
						
						// Peak or junk?
						if(nat>=global->hitfinderMinPixCount && nat<=global->hitfinderMaxPixCount) {
							
							threadInfo->peakNpix += nat;
							threadInfo->peakTotal += totI;
							
							
							// Only space to save the first NpeaksMax peaks
							// (more than this and the pattern is probably junk)
							if ( counter > global->hitfinderNpeaksMax ) {
								counter++;
								continue;
							}
							
							// Remember peak information
							threadInfo->peak_intensity[counter] = totI;
							threadInfo->peak_com_x[counter] = peak_com_x/totI;
							threadInfo->peak_com_y[counter] = peak_com_y/totI;
							threadInfo->peak_npix[counter] = nat;
							
							e = lrint(peak_com_x/totI) + lrint(peak_com_y/totI)*global->pix_nx;
							threadInfo->peak_com_index[counter] = e;
							threadInfo->peak_com_x_assembled[counter] = global->pix_x[e];
							threadInfo->peak_com_y_assembled[counter] = global->pix_y[e];
							threadInfo->peak_com_r_assembled[counter] = global->pix_r[e];
							counter++;
						}
					}
				}
			}
		}
	}	
	

    threadInfo->nPeaks = counter;
    
	// Now figure out whether this is a hit
	if(counter >= global->hitfinderNpeaks && counter <= global->hitfinderNpeaksMax)
		hit = 1;	
    
	
	free(temp);
	free(mask);
	free(inx);
	free(iny);
    
    return(hit);
	
}


/*	
 *	peakfinder 5
 *	Rick Kirian
 */
int peakfinder5(cGlobal *global, tThreadInfo	*threadInfo) {

	long  nat;
	long  lastnat;
	long  counter = 0;
	int   hit = 0;
	//float total = 0;
	int   search_x[] = {-1,0,1,-1,1,-1,0,1};
	int   search_y[] = {-1,-1,-1,0,0,1,1,1};
	int   search_n = 8;
	long  e,fs,ss;
	long  *inx = (long *) calloc(global->pix_nn, sizeof(long));
	long  *iny = (long *) calloc(global->pix_nn, sizeof(long));
	float totI;
	float peak_com_x;
	float peak_com_y;
	long  thisx;
	long  thisy;
	float grad;
	float lbg, imbg; /* local background nearby peak */
	float *lbg_buffer;
	int   fsmin, fsmax, ssmin, ssmax;
	int   lbg_ss, lbg_fs, lbg_e;
	int   thisfs, thisss;
	float mingrad = global->hitfinderMinGradient*2;
	mingrad *= mingrad;

	/* Combined mask */
	int * mask = (int *) calloc(global->pix_nn, sizeof(int) );
	memcpy(mask,global->hitfinderResMask,global->pix_nn*sizeof(int));
	for (long i=0; i<global->pix_nn; i++) mask[i] *= 
		global->hotpixelmask[i] *
		global->badpixelmask[i] *
		threadInfo->saturatedPixelMask[i];

	// zero out bad pixels in temporary intensity map
	float * temp = (float *) calloc(global->pix_nn,sizeof(float));
	for (long i=0; i<global->pix_nn; i++) temp[i] = threadInfo->corrected_data[i]*mask[i];

	// Loop over modules (8x8 array)
	for(long mj=0; mj<global->nasics_y; mj++){
	for(long mi=0; mi<global->nasics_x; mi++){	
	// Loop over pixels within a module
	for(long j=1; j<global->asic_ny-1; j++){
	for(long i=1; i<global->asic_nx-1; i++){

		ss = (j+mj*global->asic_ny);
		fs = i+mi*global->asic_nx;
		e = ss*global->pix_nx + fs;

		if ( global->hitfinderResMask[e] != 1 ) continue;

		if ( temp[e] < global->hitfinderADC ) continue;

		if ( global->hitfinderCheckGradient == 1 ){
		
			float dx1, dx2, dy1, dy2, dxs, dys;
			
			/* can't measure gradient where bad pixels present */
			if ( global->badpixelmask[e] != 1 ) continue;
			if ( global->badpixelmask[e+1] != 1 ) continue;
			if ( global->badpixelmask[e-1] != 1 ) continue;
			if ( global->badpixelmask[e+global->pix_nx] != 1 ) continue;
			if ( global->badpixelmask[e-global->pix_nx] != 1 ) continue;

			/* Get gradients */
			dx1 = temp[e] - temp[e+1];
			dx2 = temp[e-1] - temp[e];
			dy1 = temp[e] - temp[e+global->pix_nx];
			dy2 = temp[e-global->pix_nx] - temp[e];
			/* this is sort of like the mean squared gradient, times 4... */
			dxs = ((dx1*dx1) + (dx2*dx2)) ;
			dys = ((dy1*dy1) + (dy2*dy2)) ;	
			grad = dxs + dys;

			if ( grad < mingrad ) continue;
		}

		lbg = 0; /* local background value */
		if ( global->hitfinderSubtractLocalBG == 1 ) {
			int lbg_counter = 0;
			/* region nearby the peak */
			fsmin = fs - global->hitfinderLocalBGRadius;
			fsmax = fs + global->hitfinderLocalBGRadius;
			ssmin = ss - global->hitfinderLocalBGRadius;
			ssmax = ss + global->hitfinderLocalBGRadius;
			/* check module bounds */
			if ( fsmin < mi*global->asic_nx ) fsmin = mi*global->asic_nx;
			if ( fsmax >= (mi+1)*global->asic_nx ) fsmax = (mi+1)*global->asic_nx - 1; 
			if ( ssmin < mj*global->asic_ny ) ssmin = mj*global->asic_ny;
			if ( ssmax >= (mj+1)*global->asic_ny ) ssmax = (mj+1)*global->asic_ny - 1;
			/* buffer for calculating median */
			lbg_buffer = (float *) calloc((fsmax-fsmin+1)*(ssmax-ssmin+1),sizeof(float));
			/* now calculate median */
			for ( lbg_ss = ssmin; lbg_ss <= ssmax; lbg_ss++) {
			for ( lbg_fs = fsmin; lbg_fs <= fsmax; lbg_fs++ ) {
				thisss = (j+mj*global->asic_ny)*global->pix_nx;
				thisfs = i+mi*global->asic_nx;
				lbg_e = thisss + thisfs;
				/* check if we're ignoring this pixel*/ 
				if ( global->badpixelmask[lbg_e] == 1 ) {
					lbg_buffer[lbg_counter] = temp[lbg_e];
					lbg_counter++;		
				}
			}}
			if ( lbg_counter > 0 )
				lbg = kth_smallest(lbg_buffer,lbg_counter,lbg_counter/2);	
			free(lbg_buffer);
			if ( (temp[e]-lbg) > global->hitfinderADC ) continue;								
		}

		inx[0] = i;
		iny[0] = j;
		nat = 1;
		totI = 0; 
		peak_com_x = 0; 
		peak_com_y = 0; 
		int badpix = 0;

		// start counting bad pixels
		if ( mask[e] == 0 ) badpix += 1;

		// Keep looping until the pixel count within this peak does not change
		do {
			
			lastnat = nat;
			// Loop through points known to be within this peak
			for(long p=0; p<nat; p++){
				// Loop through search pattern
				for(long k=0; k<search_n; k++){

					// Array bounds check
					if((inx[p]+search_x[k]) < 0) continue;
					if((inx[p]+search_x[k]) >= global->asic_nx) continue;
					if((iny[p]+search_y[k]) < 0) continue;
					if((iny[p]+search_y[k]) >= global->asic_ny) continue;
					
					// Neighbour point 
					thisx = inx[p]+search_x[k]+mi*global->asic_nx;
					thisy = iny[p]+search_y[k]+mj*global->asic_ny;
					e = thisx + thisy*global->pix_nx;
					
					// count bad pixels within or neighboring this peak
					if ( mask[e] == 0 ) badpix += 1;

					// Above threshold?
					imbg = temp[e] - lbg; /* "intensitiy minus background" */
					if(imbg > global->hitfinderADC){
						totI += imbg; // add to integrated intensity
						peak_com_x += imbg*( (float) thisx ); // for center of mass x
						peak_com_y += imbg*( (float) thisy ); // for center of mass y
						temp[e] = 0; // zero out this intensity so that we don't count it again
						inx[nat] = inx[p]+search_x[k];
						iny[nat] = iny[p]+search_y[k];
						nat++;
					}
				}
			}
		} while(lastnat != nat);

		// Peak or junk?
		if( nat>=global->hitfinderMinPixCount 
			  && nat<=global->hitfinderMaxPixCount 
		     && badpix==0 ) {
			
			threadInfo->peakNpix += nat;
			threadInfo->peakTotal += totI;
			
			// Only space to save the first NpeaksMax peaks
			// (more than this and the pattern is probably junk)
			if ( counter > global->hitfinderNpeaksMax ) {
				threadInfo->nPeaks = counter;
				return 0;
			}
			
			// Remember peak information
			threadInfo->peak_intensity[counter] = totI;
			threadInfo->peak_com_x[counter] = peak_com_x/totI;
			threadInfo->peak_com_y[counter] = peak_com_y/totI;
			threadInfo->peak_npix[counter] = nat;

			e = lrint(peak_com_x/totI) + lrint(peak_com_y/totI)*global->pix_nx;
			threadInfo->peak_com_index[counter] = e;
			threadInfo->peak_com_x_assembled[counter] = global->pix_x[e];
			threadInfo->peak_com_y_assembled[counter] = global->pix_y[e];
			threadInfo->peak_com_r_assembled[counter] = global->pix_r[e];
			counter++;
			
		}
	}}}}	
	threadInfo->nPeaks = counter;

	/* check peak separations?  get rid of clusters? */
	if ( global->hitfinderCheckPeakSeparation == 1 ) {
		
		int peakNum;
		int peakNum1;
		int peakNum2;
		float diffX,diffY;
		float maxPeakSepSq = global->hitfinderMaxPeakSeparation*global->hitfinderMaxPeakSeparation;
		float peakSepSq;
		
		/* all peaks assumed "good" to start */
		for ( peakNum = 0; peakNum < threadInfo->nPeaks; peakNum++ )
			threadInfo->good_peaks[peakNum] = 1;
		
		/* loop through unique peak pairs, checking that they are not too close */
		for ( peakNum1 = 0; peakNum1 < threadInfo->nPeaks - 1; peakNum1++ ) {
			if ( threadInfo->good_peaks[peakNum1] == 0 ) continue;
			for (peakNum2 = peakNum1 + 1; peakNum2 < threadInfo->nPeaks; peakNum2++ ) {
				if ( threadInfo->good_peaks[peakNum2] == 0 ) continue;
				/* check the distance between these two peaks */
				diffX = threadInfo->peak_com_x[peakNum1] - threadInfo->peak_com_x[peakNum2];
				diffY = threadInfo->peak_com_y[peakNum1] - threadInfo->peak_com_y[peakNum2];
				peakSepSq = diffX*diffX + diffY*diffY;
				if ( peakSepSq < maxPeakSepSq ) {
					if (threadInfo->peak_intensity[peakNum1] > threadInfo->peak_intensity[peakNum2]) 
					threadInfo->good_peaks[peakNum2] = 0;
					else 
					threadInfo->good_peaks[peakNum2] = 0;
				}
			}
		}
		/* now repopulate the peak list with good ones */
		int gpc = 0;
		for ( peakNum = 0; peakNum < threadInfo->nPeaks; peakNum++ ) {
			if ( threadInfo->good_peaks[peakNum] == 1 ) {
				threadInfo->peak_com_x[gpc] = threadInfo->peak_com_x[peakNum];
				threadInfo->peak_com_y[gpc] = threadInfo->peak_com_y[peakNum];
				threadInfo->peak_com_x_assembled[gpc] = threadInfo->peak_com_x_assembled[peakNum];
				threadInfo->peak_com_y_assembled[gpc] = threadInfo->peak_com_y_assembled[peakNum];
				threadInfo->peak_com_r_assembled[gpc] = threadInfo->peak_com_r_assembled[peakNum];
				threadInfo->peak_com_index[gpc] = threadInfo->peak_com_index[peakNum];
				threadInfo->peak_intensity[gpc] = threadInfo->peak_intensity[peakNum];
				threadInfo->peak_npix[gpc] =threadInfo->peak_npix[peakNum];
				gpc++;
			}
		}
		counter = gpc;
	}	
	
	threadInfo->nPeaks = counter;

	// Now figure out whether this is a hit
	if(counter >= global->hitfinderNpeaks && counter <= global->hitfinderNpeaksMax)
		hit = 1;	

	free(inx); 			
	free(iny);
	free(mask);
	free(temp);

	return(hit);
	
}

/*
 *	Peak finder 6
 *	Rick Kirian
 */
int peakfinder6(cGlobal *global, tThreadInfo	*threadInfo) {
	
	int counter = 0;
	int hit = 0;
	int stride = global->pix_nx;
	int fs,ss,e,thise,p,ce,ne,nat,lastnat,cs,cf;
	int peakindex,newpeak;
	float dist, itot, ftot, stot;	
	float thisI,bgsig,snr;
	int a,b,c,d;
	
	/* For counting neighbor pixels */
	int * nexte = (int *) calloc(global->pix_nn,sizeof(int));	
	
	/* Shift in linear indices to eight nearest neighbors */
	int shift[8] = { +1, -1, +stride, -stride,
		+stride - 1, +stride + 1,
		-stride - 1, -stride + 1};
	
	/* Combined mask */
	int * mask = (int *) calloc(global->pix_nn, sizeof(int) );
	memcpy(mask,global->hitfinderResMask,global->pix_nn*sizeof(int));
	for (long i=0; i<global->pix_nn; i++) mask[i] *= 
		global->hotpixelmask[i] *
		global->badpixelmask[i] *
		threadInfo->saturatedPixelMask[i];
	
	/* Combined mask for pixel counting */
	int * natmask = (int *) calloc(global->pix_nn, sizeof(int) );
	memcpy(natmask,mask,global->pix_nn*sizeof(int));
	
	// zero out bad pixels in temporary intensity map
	float * temp = (float *) calloc(global->pix_nn,sizeof(float));
	for (long i=0; i<global->pix_nn; i++) temp[i] = threadInfo->corrected_data[i]*mask[i];
	
	// Loop over modules (8x8 array)
	for(long mj=0; mj<global->nasics_y; mj++){
		for(long mi=0; mi<global->nasics_x; mi++){	
			
			/* Some day, the local background radius may be different 
			 * for each panel.  Could even be specified for each pixel
			 * when detector geometry is determined */	
			int bgrad = global->hitfinderLocalBGRadius;
			
			int asic_min_fs = mi*global->asic_nx;
			int asic_min_ss = mj*global->asic_ny;
			
			// Loop over pixels within a module
			for(long j=bgrad; j<global->asic_ny-1-bgrad; j++){
				for(long i=bgrad; i<global->asic_nx-1-bgrad; i++){
					
					ss = asic_min_ss + j;
					fs = asic_min_fs + i;
					e = ss*stride + fs;
					
					if ( temp[e] < global->hitfinderADC ) continue;
					
					/* Check if this pixel value is larger than all of its neighbors */
					for ( int k=0; k<8; k++ ) if ( temp[e] <= temp[e+shift[k]] ) continue;
					
					/* Check SNR - first calculate background and variance within a ring 
					 * surrounding this pixel */
					int bgcount = 0;
					float bg = 0;
					float bgsq = 0;
					/* The top, right, bottom, and left starting indices for the box */
					int topstart = e - bgrad*(1+stride);
					int rightstart = e + bgrad*(stride-1);
					int bottomstart = e + bgrad*(1+stride);
					int leftstart = e + bgrad*(1-stride);
					/* We could also step through bgrad values here for a ring thickness 
					 * greater than 1.  Doesn't seem necessary; just slows things down.*/
					for (int q=0; q < bgrad*2; q++) {
						a = topstart + q*stride;
						b = rightstart + q;
						c = bottomstart - q*stride;
						d = leftstart - q;
						bgcount += mask[a] + mask[b] + mask[c] + mask[d];
						bg += temp[a] + temp[b] + temp[c] + temp[d];
						bgsq += temp[a]*temp[a] + temp[b]*temp[b] + 
						temp[c]*temp[c] + temp[d]*temp[d];
					}
					/* Skip it if there are less then 7/8 good pixels in the ring */
					if ( bgcount < 7*bgrad ) continue;
					bg = bg/bgcount;
					thisI = temp[e] - bg;
					/* Recheck intensity threshold now that background is corrected */
					if ( thisI < global->hitfinderADC ) continue;
					bgsq = bgsq/bgcount;
					bgsig = sqrt(bgsq - bg*bg);
					snr = thisI/bgsig;
					/* Check SNR threshold */
					if ( snr < global->hitfinderMinSNR ) continue;
					
					/* Count the number of connected pixels and centroid. */
					nat = 1;
					nexte[0] = e;
					ce = 0;
					itot = 0; ftot = 0; stot = 0;
					do {
						lastnat = nat;
						thise = nexte[ce];
						// if ( natmask[thise] == 0 ) goto skipme;
						for ( int k=0; k<8; k++ ) {
							/* this is the index of a neighboring pixel */
							ne = thise + shift[k];
							/* Array bounds check */
							if ( ne < 0 || ne >= global->pix_nn ) continue;
							// Check that we aren't recounting the same pixel
							if ( natmask[ne] == 0 ) continue;
							/* Check SNR condition */
							if ( (temp[ne]-bg)/bgsig > global->hitfinderMinSNR ) {
								natmask[ne] = 0;
								nexte[nat] = ne;
								nat++;
								thisI = temp[ne] - bg;
								itot += thisI;
								cf = ne % stride;
								cs = ne / stride;
								ftot += thisI*(float)cf;
								stot += thisI*(float)cs;						
							}
						}
						ce++;
					} while ( nat != lastnat );
					
					/* Final check that we satisfied the connected pixel requirement */
					if ( nat < global->hitfinderMinPixCount ) continue;
					fs = lrint(ftot/itot);
					ss = lrint(stot/itot);

					/* Have we already found better peak nearby? */
					newpeak = 1;
					peakindex = counter;
					for ( p=counter-1; p >= 0; p-- ) {
						/* Distance to neighbor peak */
						dist = pow(fs - threadInfo->peak_com_x[p],2) + 
						pow(ss - threadInfo->peak_com_y[p], 2);
						if ( dist <= global->hitfinderMaxPeakSeparation ) {
							if ( snr > threadInfo->peak_snr[p]) {
								/* This peak will overtake its neighbor */ 
								newpeak = 0;
								peakindex = p; 
								continue;
							} else {
								/* There is a better peak nearby */
								goto skipme; 
							}
						} 
					}
					
					/* Now find proper centroid */
					itot = 0; ftot = 0; stot = 0;
					for ( cs=ss-bgrad; cs<=ss+bgrad; cs++) {
						for ( cf=fs-bgrad; cf<=fs+bgrad; cf++) {
							ce = cs*stride + cf;
							if ( mask[ce] == 0 ) continue;
							thisI = temp[ce] - bg;
							itot += thisI;
							ftot += thisI*(float)cf;
							stot += thisI*(float)cs;
						}
					}		

					/* Dump peak info into thread structure, for writing hdf5 files, etc. */
					threadInfo->peak_intensity[peakindex] = itot;
					threadInfo->peak_com_x[peakindex] = ftot/itot;
					threadInfo->peak_com_y[peakindex] = stot/itot;
					threadInfo->peak_npix[peakindex] = 1;
					threadInfo->peak_snr[peakindex] = snr;
					threadInfo->peak_com_index[peakindex] = e;
					threadInfo->peak_com_x_assembled[peakindex] = global->pix_x[e];
					threadInfo->peak_com_y_assembled[peakindex] = global->pix_y[e];
					threadInfo->peak_com_r_assembled[peakindex] = global->pix_r[e];
					
					/* Note that we only increment the peak counter if this is a new one */
					if ( newpeak ) counter++;
					
					/* Have we found too many peaks? */
					if ( counter >= global->hitfinderNpeaksMax ) {
						threadInfo->nPeaks = global->hitfinderNpeaksMax;
						printf("MESSAGE: Found too many peaks - aborting peaksearch early.\n");
						hit = 0;
						goto nohit;
					}
					
				skipme:;
					
				}}
		}}	
	
	threadInfo->nPeaks = counter;
	
	if(threadInfo->nPeaks >= global->hitfinderNpeaks && 
	   threadInfo->nPeaks <= global->hitfinderNpeaksMax)
		hit = 1;
	
nohit:
	
	free(nexte);
	free(mask);
	free(natmask);
	free(temp);
	
	return(hit);
	
}

