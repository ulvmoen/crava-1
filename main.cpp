#include <iostream>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "lib/systemcall.h"
#include "lib/global_def.h"
#include "lib/segy.h"
#include "lib/timekit.hpp"

#include "nrlib/iotools/logkit.hpp"

#include "src/model.h"
#include "src/wavelet.h"
#include "src/crava.h"
#include "src/fftgrid.h"
#include "src/simbox.h"

using namespace NRLib2;

int main(int argc, char** argv)
{  
  if (argc != 2) {
    printf("Usage: %s modelfile\n",argv[0]);
    exit(1);
  }
  LogKit::SetScreenLog(LogKit::L_LOW);
  LogKit::StartBuffering();

  double wall=0.0, cpu=0.0;
  TimeKit::getTime(wall,cpu);
  LogKit::LogFormatted(LogKit::LOW,"\n***********************************************************************");
  LogKit::LogFormatted(LogKit::LOW,"\n***                                                                 ***"); 
  LogKit::LogFormatted(LogKit::LOW,"\n***                        C  R  A  V  A                            ***"); 
  LogKit::LogFormatted(LogKit::LOW,"\n***                                                                 ***"); 
  LogKit::LogFormatted(LogKit::LOW,"\n***********************************************************************\n\n");

  char segyMode[50];
  char bypassCoordScaling[50];

#ifdef SEGY_ISEX
  strcpy(segyMode,"ISEX");
#else
  strcpy(segyMode,"Seisworks/Charisma");
#endif
#ifdef BYPASS_COORDINATE_SCALING
  strcpy(bypassCoordScaling,"yes");
#else
  strcpy(bypassCoordScaling,"no");
#endif

  std::cout 
    << "Compiled: " << SystemCall::getDate() << "/" << SystemCall::getTime() << "\n"
    << std::endl;

  const char * userName    = SystemCall::getUserName();
  const char * dateAndTime = SystemCall::getCurrentTime();
  const char * hostName    = SystemCall::getHostName();
  LogKit::LogFormatted(LogKit::LOW,"Compile-time directives used in this version:\n");
  LogKit::LogFormatted(LogKit::LOW,"  SegY mode: %s\n",segyMode);
  LogKit::LogFormatted(LogKit::LOW,"  Bypass coordinate scaling: %s\n\n",bypassCoordScaling);
  LogKit::LogFormatted(LogKit::LOW,"Log written by                             : %s\n",userName);
  LogKit::LogFormatted(LogKit::LOW,"Date and time                              : %s"  ,dateAndTime);
  LogKit::LogFormatted(LogKit::LOW,"Host                                       : %s\n",hostName);
  delete [] userName;
  delete [] dateAndTime;
  delete [] hostName;

  // Parsing modelfile and reading files
  Model * model = new Model(argv[1]);
  if(model->getFailed())
  {
    LogKit::LogFormatted(LogKit::LOW,"\nErrors detected in model file processing.\nAborting.\n");
    return(1);
  }

  Crava * crava;

  if(model->getModelSettings()->getGenerateSeismic() == false)
  {
    if (model->getModelSettings()->getDoInversion())
    {
      time_t timestart, timeend;
      time(&timestart);
      
      LogKit::LogFormatted(LogKit::LOW,"\n***********************************************************************");
      LogKit::LogFormatted(LogKit::LOW,"\n***                    Building Stochastic Model                     ***"); 
      LogKit::LogFormatted(LogKit::LOW,"\n***********************************************************************\n\n");

      crava = new Crava(model);
      
      char * warningText = new char[12*MAX_STRING*crava->getNTheta()];
      
      if(crava->getWarning( warningText ) != 0)
       {
         LogKit::LogFormatted(LogKit::LOW,"\nWarning  !!!\n");
         LogKit::LogFormatted(LogKit::LOW,"%s",warningText);
         LogKit::LogFormatted(LogKit::LOW,"\n");
       }
      crava->printEnergyToScreen();
      
      time(&timeend);
      LogKit::LogFormatted(LogKit::DEBUGLOW,"\nTime elapsed :  %d\n",timeend-timestart);  
      LogKit::LogFormatted(LogKit::LOW,"\n***********************************************************************");
      LogKit::LogFormatted(LogKit::LOW,"\n***             Posterior model / Performing Inversion              ***"); 
      LogKit::LogFormatted(LogKit::LOW,"\n***********************************************************************\n\n");
      crava->computePostMeanResidAndFFTCov();
      time(&timeend);
      LogKit::LogFormatted(LogKit::DEBUGLOW,"\nTime elapsed :  %d\n",timeend-timestart);  
      
      if(model->getModelSettings()->getNumberOfSimulations() > 0)
      {
        LogKit::LogFormatted(LogKit::LOW,"\n***********************************************************************");
        LogKit::LogFormatted(LogKit::LOW,"\n***                Simulating from posterior model                  ***"); 
        LogKit::LogFormatted(LogKit::LOW,"\n***********************************************************************\n\n");
        crava->simulate(model->getRandomGen());
      }
      
      // Posterior covariance
      if((model->getModelSettings()->getOutputFlag() & ModelSettings::CORRELATION) > 0)
      {
        LogKit::LogFormatted(LogKit::LOW,"\nPost process ...\n"); 
        crava->computePostCov();
        LogKit::LogFormatted(LogKit::LOW,"\n             ... post prosess ended\n");
        
      }
      crava->computeFaciesProb();
      delete [] warningText;
      delete crava;
    } //end doinversion 
  }
  else
  {
    LogKit::LogFormatted(LogKit::LOW,"\nBuilding model ...\n");
    crava = new Crava(model);
    LogKit::LogFormatted(LogKit::LOW,"\n               ... model built\n");

    // Computing synthetic seismic
    LogKit::LogFormatted(LogKit::LOW,"\nComputing synthetic seismic ..."); 
    crava->computeSyntSeismic(crava->getpostAlpha(),crava->getpostBeta(),crava->getpostRho());
    LogKit::LogFormatted(LogKit::LOW,"                              ... synthetic seismic computed.\n");
	
    delete crava;
  } 
  delete model;

  LogKit::LogFormatted(LogKit::LOW,"\n*** CRAVA closing  ***\n"); 
  TimeKit::getTime(wall,cpu);
  LogKit::LogFormatted(LogKit::LOW,"\nTotal CPU  time used in CRAVA: %6d seconds", static_cast<int>(cpu));
  LogKit::LogFormatted(LogKit::LOW,"\nTotal Wall time used in CRAVA: %6d seconds\n", static_cast<int>(wall));
  LogKit::LogFormatted(LogKit::LOW,"\n*** CRAVA finished ***\n");

  LogKit::EndLog();
  return(0);
}
