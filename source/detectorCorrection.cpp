/*
 *  detectorCorrection.cpp
 *  cheetah
 *
 *  Created by Anton Barty on 23/11/11.
 *  Copyright 2011 CFEL. All rights reserved.
 *
 */

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



/*
 *	Subtract pre-loaded darkcal file
 */
void subtractDarkcal(tThreadInfo *threadInfo, cGlobal *global){
	
	// Do darkcal subtraction
	for(long i=0;i<global->pix_nn;i++) {
		threadInfo->corrected_data[i] -= global->darkcal[i]; 
	}
	
}


/*
 *	Apply gain correction
 *	Assumes the gaincal array is appropriately 'prepared' when loaded so that all we do is a multiplication.
 *	All that checking for division by zero (and inverting when required) needs only be done once, right? 
 */
void applyGainCorrection(tThreadInfo *threadInfo, cGlobal *global){
	
	for(long i=0;i<global->pix_nn;i++) 
		threadInfo->corrected_data[i] *= global->gaincal[i];
	
}


/*
 *	Apply bad pixel mask
 *	Assumes that all we have to do here is a multiplication.
 */
void applyBadPixelMask(tThreadInfo *threadInfo, cGlobal *global){
	
	for(long i=0;i<global->pix_nn;i++) 
		threadInfo->corrected_data[i] *= global->badpixelmask[i];
	
}



/*
 *	Subtract common mode on each module
 *	Common mode is the kth lowest pixel value in the whole ASIC (similar to a median calculation)
 */
void cmModuleSubtract(tThreadInfo *threadInfo, cGlobal *global){
	
	DEBUGL2_ONLY printf("cmModuleSubtract\n");
	
	long		e;
	long		mval;
    long		counter;
	float		median;
	
	float	*buffer; 
	buffer = (float*) calloc(global->asic_nx*global->asic_ny, sizeof(float));
    
	//mval = lrint((global->asic_nx*global->asic_ny)*global->cmFloor);
	
	// Loop over modules (8x8 array)
	for(long mi=0; mi<global->nasics_x; mi++){
		for(long mj=0; mj<global->nasics_y; mj++){
			
			// Zero array
			for(long i=0; i<global->asic_nx*global->asic_ny; i++)
				buffer[i] = 0;
			
			// Loop over pixels within a module
            counter = 0;
			for(long j=0; j<global->asic_ny; j++){
				for(long i=0; i<global->asic_nx; i++){
					e = (j + mj*global->asic_ny) * (global->asic_nx*global->nasics_x);
					e += i + mi*global->asic_nx;
                    if(global->badpixelmask[e] != 0) {           // badpixelmask[e]==0 are the bad pixels
						buffer[counter++] = threadInfo->corrected_data[e];
					}
				}
			}
			
			
            // Calculate background using median value 
			//median = kth_smallest(buffer, global->asic_nx*global->asic_ny, mval);
			if(counter>0) {
				mval = lrint(counter*global->cmFloor);
                if(mval < 0) 
                    mval = 1;
				median = kth_smallest(buffer, counter, mval);
			}
			else 
				median = 0;

			// Subtract median value
			for(long j=0; j<global->asic_ny; j++){
				for(long i=0; i<global->asic_nx; i++){
					e = (j + mj*global->asic_ny) * (global->asic_nx*global->nasics_x);
					e += i + mi*global->asic_nx;
					threadInfo->corrected_data[e] -= median;
				}
			}
		}
	}
	free(buffer);
}



/*
 *	Subtract offset estimated from unbonded pixels
 *	In the upstream detector, the unbonded pixels are in Q0:0-3 and Q2:4-5 and are at the 
 *	corners of each asic and at row=col (row<194) or row-194==col (row>194) for col%10=0.  
 */
void cmSubtractUnbondedPixels(tThreadInfo *threadInfo, cGlobal *global){
	DEBUGL2_ONLY printf("cmModuleSubtract\n");
	
	long		e;
	double		counter;
	double		background;
	
	
	// Loop over modules (8x8 array)
	for(long mi=0; mi<global->nasics_x; mi++){
		for(long mj=0; mj<global->nasics_y; mj++){
			
			// Only asics in Q0:0-3 and Q2:4-5 are unbonded
			if( ! ((mi<=1 && mj<=3) || (mi >= 4 && mi<=5 && mj >= 4 && mj<=5)) )
				continue;
			
			
			// Loop over unbonded pixels within each ASIC
			background = 0.0;
			counter = 0.0;
			for(long j=0; j<global->asic_ny-1; j+=10){
				long i=j;
				e = (j + mj*global->asic_ny) * (global->asic_nx*global->nasics_x);
				e += i + mi*global->asic_nx;
				background += threadInfo->corrected_data[e];
				counter += 1;
			}
			background /= counter;
			
			//printf("%f ",background);
			
			// Subtract background from entire ASIC
			for(long j=0; j<global->asic_ny; j++){
				for(long i=0; i<global->asic_nx; i++){
					e = (j + mj*global->asic_ny) * (global->asic_nx*global->nasics_x);
					e += i + mi*global->asic_nx;
					threadInfo->corrected_data[e] -= background;
					
				}
			}
		}
	}
	
}

/*
 *	Subtract common mode estimated from signal behind wires
 *	Common mode is the kth lowest pixel value in the whole ASIC (similar to a median calculation)
 */
void cmSubtractBehindWires(tThreadInfo *threadInfo, cGlobal *global){
	
	DEBUGL2_ONLY printf("cmModuleSubtract\n");
	
	long		p;
	long		counter;
	long		mval;
	float		median;
	
	float	*buffer; 
	buffer = (float*) calloc(global->asic_ny*global->asic_nx, sizeof(float));
	
	// Loop over modules (8x8 array)
	for(long mi=0; mi<global->nasics_x; mi++){
		for(long mj=0; mj<global->nasics_x; mj++){
			
			
			// Loop over pixels within a module, remembering signal behind wires
			counter = 0;
			for(long j=0; j<global->asic_ny; j++){
				for(long i=0; i<global->asic_nx; i++){
					p = (j + mj*global->asic_ny) * (global->asic_nx*global->nasics_x);
					p += i + mi*global->asic_nx;
					if(global->wiremask[i] && global->badpixelmask[p] != 0) {
						buffer[counter] = threadInfo->corrected_data[p];
						counter++;
					}
				}
			}
			
			// Median value of pixels behind wires
			if(counter>0) {
				mval = lrint(counter*global->cmFloor);
				median = kth_smallest(buffer, counter, mval);
			}
			else 
				median = 0;
			
			
			// Subtract median value
			for(long i=0; i<global->asic_nx; i++){
				for(long j=0; j<global->asic_ny; j++){
					p = (j + mj*global->asic_ny) * (global->asic_nx*global->nasics_x);
					p += i + mi*global->asic_nx;
					threadInfo->corrected_data[p] -= median;
				}
			}
		}
	}
	free(buffer);
}


/*
 *	Identify and kill hot pixels
 */
void killHotpixels(tThreadInfo *threadInfo, cGlobal *global){
	
	
	// First update global hot pixel buffer
	int16_t	*buffer = (int16_t *) calloc(global->pix_nn,sizeof(int16_t));
	for(long i=0;i<global->pix_nn;i++){
		buffer[i] = (fabs(threadInfo->corrected_data[i])>global->hotpixADC)?(1):(0);
	}
	pthread_mutex_lock(&global->hotpixel_mutex);
	long frameID = global->hotpixCounter%global->hotpixMemory;	
	memcpy(global->hotpix_buffer+global->pix_nn*frameID, buffer, global->pix_nn*sizeof(int16_t));
	global->hotpixCounter += 1;
	pthread_mutex_unlock(&global->hotpixel_mutex);
	free(buffer);
	
	
	// Apply the current hot pixel mask 
	for(long i=0;i<global->pix_nn;i++){
		threadInfo->corrected_data[i] *= global->hotpixelmask[i];
	}
	threadInfo->nHot = global->nhot;
	
	
	
}


void calculateHotPixelMask(cGlobal *global){
	
	long	cutoff = lrint((global->hotpixMemory*global->hotpixFreq));
	printf("Recalculating hot pixel mask at %li/%i\n",cutoff,global->hotpixMemory);	
	
	// Loop over all pixels 
	long	counter;
	long	nhot;
	for(long i=0; i<global->pix_nn; i++) {
		
		counter = 0;
		for(long j=0; j< global->hotpixMemory; j++) {
			counter += global->hotpix_buffer[j*global->pix_nn+i]; 
		}
		
		// Apply threshold
		if(counter < cutoff) {
			global->hotpixelmask[i] = 1;
		}
		else{
			global->hotpixelmask[i] = 0;
			nhot++;				
		}		
	}	
	global->nhot = nhot;
	global->last_hotpix_update = global->hotpixCounter;
}




