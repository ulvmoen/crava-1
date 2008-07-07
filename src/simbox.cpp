#include <math.h>
#include <assert.h>
#include <algorithm>

#include "nrlib/volume/volume.hpp"
#include "nrlib/surface/surfaceio.hpp"

#include "nrlib/iotools/logkit.hpp"

#include "src/simbox.h"
#include "src/model.h"


using namespace NRLib2;

Simbox::Simbox(void) : Volume()
{
  status_      = EMPTY;
  topName_     = "";
  botName_     = "";
  inLine0_     = 0;
  crossLine0_  = 0;
  ilStep_      = 1;
  xlStep_      = 1;
  constThick_  = true;
  minRelThick_ = 1.0;
  dz_          = 0;
}

Simbox::Simbox(double x0, double y0, RegularSurface<double> * z0, double lx, 
               double ly, double lz, double rot, double dx, double dy, double dz) :
  Volume()
{
  status_      = BOXOK;
  topName_     = "";
  botName_     = "";
  SetDimensions(x0,y0,lx,ly);
  SetAngle(rot);
  
  RegularSurface<double> * z1 = new RegularSurface<double>(*z0);
  z1->Add(lz);
  SetSurfaces(z0,z1); //Automatically sets lz correct in this case.

  cosrot_      = cos(rot);
  sinrot_      = sin(rot);
  dx_          = dx;
  dy_          = dy;
  dz_          = dz;
  nx_          = int(0.5+lx/dx_);
  ny_          = int(0.5+ly/dy_);
  nz_          = int(0.5+lz/dz_);
  inLine0_     = 0;
  crossLine0_  = 0;
  constThick_  = true;
  minRelThick_ = 1.0;
}

Simbox::Simbox(const Simbox *simbox) : 
  Volume(*simbox)
{
  status_      = simbox->status_;
  cosrot_      = cos(GetAngle());
  sinrot_      = sin(GetAngle());
  dx_          = simbox->dx_;
  dy_          = simbox->dy_;
  dz_          = simbox->dz_;
  nx_          = simbox->nx_;
  ny_          = simbox->ny_;
  nz_          = simbox->nz_;
  inLine0_     = simbox->inLine0_;
  crossLine0_  = simbox->crossLine0_;
  constThick_  = simbox->constThick_;
  minRelThick_ = simbox->minRelThick_;

  topName_ = new char [MAX_STRING]; 
  botName_ = new char [MAX_STRING]; 

  topName_ = simbox->topName_;
  botName_ = simbox->botName_;
}   

Simbox::~Simbox()
{
}

int
Simbox::getIndex(double x, double y, double z) const

{
  int index = IMISSING;
  int i, j, k;
  getIndexes(x,y,z,i,j,k);
  if(k != IMISSING && j != IMISSING && i != IMISSING)
    index = int(i+j*nx_+k*nx_*ny_);
  return(index);
}

int
Simbox::getClosestZIndex(double x, double y, double z)
{
  int index = IMISSING;
  int i, j, k;
  getIndexesFull(x,y,z,i,j,k);
  if(i >=0 && i < nx_ && j >=0 && j < ny_)
  {
    if(k < 0)
      k = 0;
    else if(k >= nz_)
      k = nz_-1;
    index = i+j*nx_+k*nx_*ny_;
  }
  return(index);
}

void 
Simbox::getIndexes(double x, double y, double z, int & xInd, int & yInd, int & zInd) const
{
  xInd = IMISSING;
  yInd = IMISSING;
  zInd = IMISSING;
  double rx =  (x-GetXMin())*cosrot_ + (y-GetYMin())*sinrot_;
  double ry = -(x-GetXMin())*sinrot_ + (y-GetYMin())*cosrot_;
  if(rx > 0 && rx < GetLX() && ry>0 && ry < GetLY())
  {
    double zBot, zTop = GetTopSurface().GetZ(x,y);
    if(GetTopSurface().IsMissing(zTop) == false)
    {
      zBot = GetBotSurface().GetZ(x,y);
      if(GetBotSurface().IsMissing(zBot) == false &&  z > zTop && z < zBot)
      {
        xInd = int(floor(rx/dx_));
        yInd = int(floor(ry/dy_));
        zInd = int(floor(static_cast<double>(nz_)*(z-zTop)/(zBot-zTop)));
        //LogKit::LogFormatted(LogKit::LOW,"rx,dx,xInd = %.4f %.4f %d   ry,dy,yInd = %.4f %.4f %d    %d\n",rx,dx_,xInd,ry,dy_,yInd,zInd);
      }
    }
  }
}

void 
Simbox::getIndexesFull(double x, double y, double z, int & xInd, int & yInd, int & zInd) const
{
  double rx =  (x-GetXMin())*cosrot_ + (y-GetYMin())*sinrot_;
  double ry = -(x-GetXMin())*sinrot_ + (y-GetYMin())*cosrot_;
  xInd = int(floor(rx/dx_));
  yInd = int(floor(ry/dy_));
  zInd = IMISSING;
  double zBot, zTop = GetTopSurface().GetZ(x,y);
  if(GetTopSurface().IsMissing(zTop) == false)
  {
    zBot = GetBotSurface().GetZ(x,y);
    if(GetBotSurface().IsMissing(zBot) == false)
      zInd = int(floor(static_cast<double>(nz_)*(z-zTop)/(zBot-zTop)));
  }
}

void
Simbox::getZInterpolation(double x, double y, double z, 
                          int & index1, int & index2, double & t) const
{
  double rx =  (x-GetXMin())*cosrot_ + (y-GetYMin())*sinrot_;
  double ry = -(x-GetXMin())*sinrot_ + (y-GetYMin())*cosrot_;
  int xInd = int(floor(rx/dx_));
  int yInd = int(floor(ry/dy_));
  int zInd2, zInd1;
  index1 = IMISSING;
  double zBot, zTop = GetTopSurface().GetZ(x,y);
  if(GetTopSurface().IsMissing(zTop) == false)
  {
    zBot = GetBotSurface().GetZ(x,y);
    if(GetBotSurface().IsMissing(zBot) == false)
    {
      double dz = (zBot-zTop)/static_cast<double>(nz_);
      zInd1 = static_cast<int>(floor((z-zTop)/dz)-0.5); //Find cell center above.
      if(zInd1 >=0 && zInd1 < nz_-1)
      {
        t = (z-zTop)/dz - 0.5 - static_cast<double>(zInd1);
        zInd2 = zInd1+1;
      }
      else 
      {
        t = 0;
        if(zInd1 < 0)
          zInd1 = 0;
        else
          zInd1 = nz_-1;
        zInd2 = zInd1;
      }
      index1 = xInd+yInd*nx_+zInd1*nx_*ny_;
      index2 = xInd+yInd*nx_+zInd2*nx_*ny_;
    }
  }
}

void  
Simbox::getCoord(int xInd, int yInd, int zInd, double &x, double &y, double &z) const
{
  double rx = (static_cast<double>(xInd) + 0.5)*dx_;
  double ry = (static_cast<double>(yInd) + 0.5)*dy_;
  x = rx*cosrot_-ry*sinrot_ + GetXMin();
  y = rx*sinrot_+ry*cosrot_ + GetYMin();
  z = RMISSING;
  double zBot, zTop = GetTopSurface().GetZ(x,y);
  if(GetTopSurface().IsMissing(zTop) == false)
  {
    zBot = GetBotSurface().GetZ(x,y);
    if(GetBotSurface().IsMissing(zBot) == false)
    {
      double dz = (zBot-zTop)/static_cast<double>(nz_);
      z = zTop + (static_cast<double>(zInd) + 0.5)*dz;
    }
  }
}


void
Simbox::getMinMaxZ(double &minZ, double &maxZ) const
{
  minZ = GetTopSurface().Min();
  maxZ = GetTopSurface().Min();
}

int
Simbox::isInside(double x, double y) const
{
  double rx =  (x-GetXMin())*cosrot_ + (y-GetYMin())*sinrot_;
  double ry = -(x-GetXMin())*sinrot_ + (y-GetYMin())*cosrot_;
  if(rx < 0 || rx > GetLX() || ry<0 || ry > GetLY())
    return(0);
  else
    return(1);
}

int
Simbox::insideRectangle(double xr, double yr, double rotr, double lxr, double lyr) const
{
  int allOk = 1;
  double cosrotr = cos(rotr);
  double sinrotr = sin(rotr);
  double x = GetXMin();
  double y = GetYMin();
  double rx =  (x-xr)*cosrotr + (y-yr)*sinrotr;
  double ry = -(x-xr)*sinrotr + (y-yr)*cosrotr;
  if(rx < -0.01*dx_ || rx > lxr+0.01*dx_ || ry<-0.01*dy_ || ry > lyr+0.01*dy_)
    allOk = 0;

  x = GetXMin()+GetLX()*cosrot_;
  y = GetYMin()+GetLX()*sinrot_;
  rx =  (x-xr)*cosrotr + (y-yr)*sinrotr;
  ry = -(x-xr)*sinrotr + (y-yr)*cosrotr;
  if(rx < -0.01*dx_ || rx > lxr+0.01*dx_ || ry<-0.01*dy_ || ry > lyr+0.01*dy_)
    allOk = 0;

  x = GetXMin()-GetLY()*sinrot_;
  y = GetYMin()+GetLY()*cosrot_;
  rx =  (x-xr)*cosrotr + (y-yr)*sinrotr;
  ry = -(x-xr)*sinrotr + (y-yr)*cosrotr;
  if(rx < -0.01*dx_ || rx > lxr+0.01*dx_ || ry<-0.01*dy_ || ry > lyr+0.01*dy_)
    allOk = 0;

  x = GetXMin()+GetLX()*cosrot_-GetLY()*sinrot_;
  y = GetYMin()+GetLX()*sinrot_+GetLY()*cosrot_;
  rx =  (x-xr)*cosrotr + (y-yr)*sinrotr;
  ry = -(x-xr)*sinrotr + (y-yr)*cosrotr;
  if(rx < -0.01*dx_ || rx > lxr+0.01*dx_ || ry<-0.01*dy_ || ry > lyr+0.01*dy_)
    allOk = 0;
  if(allOk == 0)
  {
    LogKit::LogFormatted(LogKit::LOW,"\n             X0         Y0              DeltaX       DeltaY    Angle\n");
    LogKit::LogFormatted(LogKit::LOW,"---------------------------------------------------------------------\n");
    LogKit::LogFormatted(LogKit::LOW,"Area:    %11.2f %11.2f   %11.2f %11.2f   %8.3f\n", GetXMin(), GetYMin(), GetLX(), GetLY(), (GetAngle()*180)/PI);
    LogKit::LogFormatted(LogKit::LOW,"Seismic: %11.2f %11.2f   %11.2f %11.2f   %8.3f\n", xr, yr, lxr, lyr, (rotr*180/PI));
    LogKit::LogFormatted(LogKit::LOW,"\nCorner     XY Area                    XY Seismic\n");
    LogKit::LogFormatted(LogKit::LOW,"-----------------------------------------------------------\n");
    LogKit::LogFormatted(LogKit::LOW,"A %18.2f %11.2f    %11.2f %11.2f\n", GetXMin(),GetYMin(), xr,yr);
    LogKit::LogFormatted(LogKit::LOW,"B %18.2f %11.2f    %11.2f %11.2f\n", GetXMin()+GetLX()*cosrot_, GetYMin()+GetLX()*sinrot_,
      xr+lxr*cosrotr, yr+lxr*sinrotr);
    LogKit::LogFormatted(LogKit::LOW,"C %18.2f %11.2f    %11.2f %11.2f\n", GetXMin()-GetLY()*sinrot_, GetYMin()+GetLY()*cosrot_,
      xr -lyr*sinrotr, yr +lyr*cosrotr);
    LogKit::LogFormatted(LogKit::LOW,"D %18.2f %11.2f    %11.2f %11.2f\n", 
      GetXMin()+GetLX()*cosrot_-GetLY()*sinrot_, GetYMin()+GetLX()*sinrot_+GetLY()*cosrot_,
      xr +lxr*cosrotr-lyr*sinrotr, yr +lxr*sinrotr+lyr*cosrotr);
  }

  //
  // Calculate and write the largest possible AREA based on the (dx, dy, angle) given by user.
  //
  // Not implemented...

  return(allOk);
}


double
Simbox::getTop(double x, double y) const
{
  double zTop = GetTopSurface().GetZ(x, y);
  if(GetTopSurface().IsMissing(zTop))
    zTop = RMISSING;
  return(zTop);
}

double
Simbox::getBot(double x, double y) const
{
  double zBot = GetBotSurface().GetZ(x, y);
  if(GetBotSurface().IsMissing(zBot))
    zBot = RMISSING;
  return(zBot);
}

char *
Simbox::getStormHeader(int cubetype, int nx, int ny, int nz, bool flat, bool ascii) const
{
  if(flat == false)
    assert(topName_ != "");
  char * header = new char[500];
  if(ascii == false)
    sprintf(header,"storm_petro_binary\n");
  else
    sprintf(header,"storm_petro_ascii\n");

  sprintf(header,"%s0 %d %f\n",  header, cubetype, RMISSING);
  sprintf(header,"%sFFTGrid\n",header);
  if(flat == false)
    sprintf(header,"%s%f %f %f %f %s %s 0.0 0.0\n", header, GetXMin(), GetLX(), 
    GetYMin(), GetLY(), topName_.c_str(), botName_.c_str());
  else
    sprintf(header,"%s%f %f %f %f 0.0 %f 0.0 0.0\n", header, GetXMin(), GetLX(), 
    GetYMin(), GetLY(), GetLZ());

  sprintf(header,"%s%f %f\n\n", header, GetLZ(), GetAngle()*180/PI);
  sprintf(header,"%s%d %d %d\n", header, nx, ny, nz);
  return(header);
}

//NBNB Ragnar: Drep char * her ved bytte av logkit.
void
Simbox::writeTopBotGrids(const char * topname, const char * botname)
{
  char dirsep = '/';
#ifdef _WINDOWS
  dirsep = '\\';
#endif

  char * tmpName = ModelSettings::makeFullFileName(topname);
  std::string tName(tmpName);
  assert(typeid(GetTopSurface()) == typeid(RegularSurface<double>));
  const RegularSurface<double> & wtsurf = 
    dynamic_cast<const RegularSurface<double> &>(GetTopSurface());
  NRLib2::WriteStormBinarySurf(wtsurf, tName);

  //Strip away path
  int i;
  for(i = strlen(tmpName)-1;i >=0; i--)
    if(tmpName[i] == dirsep)
      break;
  if(topName_ == "")
  {
    char * tmpTopName = new char[strlen(tmpName)-i+1];
    strcpy(tmpTopName, &(tmpName[i+1]));
    topName_ = std::string(tmpTopName);
    delete [] tmpTopName;
  }
  delete [] tmpName;

  tmpName =ModelSettings::makeFullFileName(botname);
  if(botName_ == "")
    {
      char * tmpBotName = new char[strlen(tmpName)-i+1];
      strcpy(tmpBotName, &(tmpName[i+1]));
      botName_ = std::string(tmpBotName);
      delete [] tmpBotName;
    }
  
  std::string bName(tmpName);
  assert(typeid(GetBotSurface()) == typeid(RegularSurface<double>));
  const RegularSurface<double> & wbsurf = 
    dynamic_cast<const RegularSurface<double> &>(GetTopSurface());
  NRLib2::WriteStormBinarySurf(wbsurf, bName);
  delete [] tmpName;
}


int
Simbox::checkError(double lzLimit, char * errText)
{
  if(status_ == NODEPTH || status_ == EMPTY)
    status_ = EXTERNALERROR; //At this stage, lack of depth is an error

  if(status_ == EXTERNALERROR || status_ == INTERNALERROR)
    //Earlier internal errors are external for this purpose.
    return(EXTERNALERROR);

  if(status_ == NOAREA)
    return(BOXOK);

  if(dz_ < 0)
  {
    double z0, z1 = 0.0;
    double x, y, rx, ry = 0.5f*dy_;
    double lzCur, lzMin = double(1e+30);
    int i,j;
    for(j=0;j<ny_;j++)
    {
      rx = 0.5f*dx_;
      for(i=0;i<nx_;i++)
      {
        x = rx*cosrot_-ry*sinrot_ + GetXMin();
        y = rx*sinrot_+ry*cosrot_ + GetYMin();
        z0 = GetTopSurface().GetZ(x,y);
        z1 = GetBotSurface().GetZ(x,y);
        if(GetTopSurface().IsMissing(z0) == false && GetBotSurface().IsMissing(z1) == false )
        {
          lzCur = z1 - z0;
          if(lzCur < lzMin)
            lzMin = lzCur;
        }
        rx += dx_;
      }
      ry += dy_;
    }
  
    if(lzMin < 0.0)
    {
      status_ = INTERNALERROR;
      sprintf(errText,"At least parts of the Top surface is lower than the base surface. Are surfaces given in wrong order?\n");
    }
    else
    {
      double lzFac = lzMin/GetLZ();
      minRelThick_ = lzFac;
      if(lzFac < lzLimit) 
      {
        status_ = INTERNALERROR;
        sprintf(errText,"Error with top/bottom grids. Minimum thickness should be at least %f times maximum, is %f.\n", lzLimit, lzFac);
      }
      else 
      {
        dz_ = GetLZ()/static_cast<double>(nz_);
      }
    }
  }
  return(status_);
}


void
Simbox::setArea(double x0, double y0, double lx, double ly, double rot, double dx, double dy)
{
  SetDimensions(x0,y0,lx,ly);
  SetAngle(rot);
  cosrot_ = cos(rot);
  sinrot_ = sin(rot);
  dx_     = dx;
  dy_     = dy;
  nx_     = int(0.5+lx/dx_);
  ny_     = int(0.5+ly/dy_);
  if(status_ == EMPTY)
    status_ = NODEPTH;
  else if(status_ == NOAREA)
    status_ = BOXOK;
}


void
Simbox::setDepth(RegularSurface<double> * zref, double zShift, double lz, 
                 double dz)
{
  zref->Add(zShift);
  RegularSurface<double> * zBot = new RegularSurface<double>(*zref);
  zBot->Add(lz);
  SetSurfaces(zref,zBot);
  dz_ = dz;
  nz_ = int(0.5+lz/dz_);
  if(status_ == EMPTY)
    status_ = NOAREA;
  else if(status_ == NODEPTH)
    status_ = BOXOK;
}


void
Simbox::setDepth(RegularSurface<double> * z0, 
                 RegularSurface<double> * z1, int nz)
{
  SetSurfaces(z0, z1);
  nz_ = nz;
  dz_ = -1;
  if(status_ == EMPTY)
    status_ = NOAREA;
  else if(status_ == NODEPTH)
    status_ = BOXOK;

  constThick_ = false;
}


void
Simbox::setSeisLines(int il0, int xl0, int ilStep, int xlStep)
{
  inLine0_ = il0;
  crossLine0_ = xl0;
  ilStep_ = ilStep;
  xlStep_ = xlStep;
}


double
Simbox::getAvgRelThick(void) const
{
  double avgThick = 0.0f;
  for (int i = 0 ; i < nx_ ; i++) {
    for (int j = 0 ; j < ny_ ; j++) {
      avgThick += getRelThick(i, j);
    }
  }
  avgThick /= nx_*ny_; 
  return avgThick;
}

double
Simbox::getRelThick(int i, int j) const
{
  double rx = (static_cast<double>(i) + 0.5)*dx_;
  double ry = (static_cast<double>(j) + 0.5)*dy_;
  double x = rx*cosrot_-ry*sinrot_ + GetXMin();
  double y = rx*sinrot_+ry*cosrot_ + GetYMin();
  return(getRelThick(x, y));
}

double
Simbox::getRelThick(double x, double y) const
{
  double relThick = 1; //Default value to be used outside grid.
  double zTop = GetTopSurface().GetZ(x,y);
  double zBot = GetBotSurface().GetZ(x,y);
  if(GetTopSurface().IsMissing(zTop) == false && 
     GetBotSurface().IsMissing(zBot) == false)
    relThick = (zBot-zTop)/GetLZ();
  return(relThick);
}
