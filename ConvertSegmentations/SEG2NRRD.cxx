#include "dcmtk/config/osconfig.h"   // make sure OS specific configuration is included first
#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/oflog/oflog.h"
#include "dcmtk/ofstd/ofconapp.h"
#include "dcmtk/dcmseg/segdoc.h"
#include "dcmtk/dcmseg/segment.h"
#include "dcmtk/dcmfg/fginterface.h"
#include "dcmtk/dcmiod/iodutil.h"
#include "dcmtk/dcmiod/modmultiframedimension.h"
#include "dcmtk/dcmdata/dcsequen.h"

#include "dcmtk/dcmfg/fgderimg.h"
#include "dcmtk/dcmfg/fgplanor.h"
#include "dcmtk/dcmfg/fgpixmsr.h"
#include "dcmtk/dcmfg/fgfracon.h"
#include "dcmtk/dcmfg/fgplanpo.h"

#include "dcmtk/oflog/loglevel.h"

#include "vnl/vnl_cross.h"

#define INCLUDE_CSTDLIB
#define INCLUDE_CSTRING
#include "dcmtk/ofstd/ofstdinc.h"

#include <sstream>

#ifdef WITH_ZLIB
#include <zlib.h>                     /* for zlibVersion() */
#endif

// ITK includes
#include <itkImageFileWriter.h>
#include <itkLabelImageToLabelMapFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkChangeInformationImageFilter.h>

// CLP inclides
#include "SEG2NRRDCLP.h"

static OFLogger dcemfinfLogger = OFLog::getLogger("qiicr.apps.iowa1");

#define CHECK_COND(condition) \
    do { \
        if (condition.bad()) { \
            OFLOG_FATAL(dcemfinfLogger, condition.text() << " in " __FILE__ << ":" << __LINE__ ); \
            throw -1; \
        } \
    } while (0)

double distanceBwPoints(vnl_vector<double> from, vnl_vector<double> to){
  return sqrt((from[0]-to[0])*(from[0]-to[0])+(from[1]-to[1])*(from[1]-to[1])+(from[2]-to[2])*(from[2]-to[2]));
}

int main(int argc, char *argv[])
{
  PARSE_ARGS;

  typedef short PixelType;
  typedef itk::Image<PixelType,3> ImageType;

  dcemfinfLogger.setLogLevel(dcmtk::log4cplus::OFF_LOG_LEVEL);

  DcmFileFormat segFF;
  DcmDataset *segDataset = NULL;
  if(segFF.loadFile(inputSEGFileName.c_str()).good()){
    segDataset = segFF.getDataset();
  } else {
    std::cerr << "Failed to read input " << std::endl;
    return -1;
  }

  OFCondition cond;
  DcmSegmentation *segdoc = NULL;
  cond = DcmSegmentation::loadFile(inputSEGFileName.c_str(), segdoc);
  if(!segdoc){
    std::cerr << "Failed to load seg! " << cond.text() << std::endl;
    return -1;
  }

  // To create an ITK image, we need:
  //  * size
  //  * origin
  //  * spacing
  //  * directions
  /*
  IODMultiframeDimensionModule segdim = segdoc->getDimensions();
  OFVector<IODMultiframeDimensionModule::DimensionIndexItem*> dimIndexList;
  OFVector<IODMultiframeDimensionModule::DimensionOrganizationItem*> dimOrgList;

  dimIndexList = segdim.getDimensionIndexSequence();
  dimOrgList = segdim.getDimensionOrganizationSequence();
  */

  DcmSegment *segment = segdoc->getSegment(1);
  FGInterface &fgInterface = segdoc->getFunctionalGroups();

  vnl_vector<double> dirX, dirY, dirZ;
  dirX.set_size(3);
  dirY.set_size(3);
  dirZ.set_size(3);

  ImageType::PointType origin;
  ImageType::RegionType imageRegion;
  ImageType::SizeType imageSize;
  ImageType::SpacingType spacing;
  ImageType::Pointer segImage = ImageType::New();

  {
    OFString str;
    if(segDataset->findAndGetOFString(DCM_Rows, str).good()){
      imageSize[0] = atoi(str.c_str());
    }
    if(segDataset->findAndGetOFString(DCM_Columns, str).good()){
      imageSize[1] = atoi(str.c_str());
    }
  }

  ImageType::DirectionType dir;

  {
    // For directions, we can only handle segments that have patient orientation
    //  identical for all frames, so either find it in shared FG, or fail
    // TODO: handle the situation when FoR is not initialized
    FGPlaneOrientationPatient *planorfg = OFstatic_cast(FGPlaneOrientationPatient*,
                                                        fgInterface.getShared(DcmFGTypes::EFG_PLANEORIENTPATIENT));
    if(!planorfg){
      std::cerr << "Plane Orientation (Patient) is missing, cannot parse input " << std::endl;
      return -1;
    }
    OFString orientStr;
    for(int i=0;i<3;i++){
      if(planorfg->getImageOrientationPatient(orientStr, i).good()){
        dirX[i] = atof(orientStr.c_str());
      } else {
        std::cerr << "Failed to get orientation " << i << std::endl;
      }
    }
    for(int i=3;i<6;i++){
      if(planorfg->getImageOrientationPatient(orientStr, i).good()){
        dirY[i-3] = atof(orientStr.c_str());
      } else {
        std::cerr << "Failed to get orientation " << i << std::endl;
      }
    }
    dirZ = vnl_cross_3d(dirX, dirY);
    dirZ.normalize();

    for(int i=0;i<3;i++){
      dir[0][i] = dirX[i];
      dir[1][i] = dirY[i];
      dir[2][i] = dirZ[i];
    }
  }

  // Size: Rows/Columns can be read directly from the respective attributes
  // For number of slices, consider that all segments must have the same number of frames.
  //   If we have FoR UID initialized, this means every segment should also have Plane
  //   Position (Patient) initialized. So we can get the number of slices by looking
  //   how many per-frame functional groups a segment has.

  std::map<double, vnl_vector<double> > sliceOriginPoints;
  std::vector<double> originDistances;
  for(int i=0;;i++){

    FGPlanePosPatient *planposfg = OFstatic_cast(FGPlanePosPatient*,
                                                    fgInterface.getPerFrame(i, DcmFGTypes::EFG_PLANEPOSPATIENT));
    if(!planposfg){
        break;
    }
    //std::cout << "Found plane position for frame " << i << std::endl;

    vnl_vector<double> sOrigin;
    sOrigin.set_size(3);
    for(int j=0;j<3;j++){
      OFString planposStr;
      if(planposfg->getImagePositionPatient(planposStr, j).good()){
        sOrigin[j] = atof(planposStr.c_str());
      } else {
        std::cerr << "Failed to read patient position" << std::endl;
      }
    }

    if(i==0)
      sliceOriginPoints[0] = sOrigin;
    else {
      double dist = dot_product(dirZ,sOrigin);
      sliceOriginPoints[dist] = sOrigin;
      originDistances.push_back(dist);
    }
  }

  std::sort(originDistances.begin(), originDistances.end());

  origin[0] = sliceOriginPoints[originDistances[0]][0];
  origin[1] = sliceOriginPoints[originDistances[0]][1];
  origin[2] = sliceOriginPoints[originDistances[0]][2];

  std::cout << "Origin: " << origin << std::endl;
  imageSize[2] = sliceOriginPoints.size();
  imageRegion.SetSize(imageSize);
  segImage->SetRegions(imageRegion);
  segImage->SetOrigin(origin);


  FGPixelMeasures *pixm = OFstatic_cast(FGPixelMeasures*,
                                                      fgInterface.getShared(DcmFGTypes::EFG_PIXELMEASURES));
  if(!pixm){
    std::cerr << "Pixel spacing is missing, cannot parse input " << std::endl;
    return -1;
  }

  OFString spacingStr;
  pixm->getPixelSpacing(spacingStr, 0);
  spacing[0] = atof(spacingStr.c_str());
  pixm->getPixelSpacing(spacingStr, 1);
  spacing[1] = atof(spacingStr.c_str());
  spacing[2] = fabs(originDistances[0]-originDistances[1]);

  std::cout << "Spacing: " << spacing << std::endl;

  segImage->Allocate();
  segImage->FillBuffer(0);

  // TODO: sort origins, calculate slice thickness, take the origin of the first
  //   slice as the volume origin -- can we reuse the code in ITK for this?

  // TODO:
  //  -- consider that if FoR UID is not present, we need to derive this from DerivationImageSequence
  //  -- need to think what we will have if the derivation image is a multiframe itself
  //  -- in the general case, can have multiple dimension organizations?

  // Paste frames for each segment into the recovered image raster
  // Possible scenarios (?):
  //  1) we have only one stack
  //      * StackID and InStackPositionNumber are optional, cannot rely on it
  //      * use PatientPositionSequence>ImagePositionPatient to
  //         find the matching slice (need to deal with numeric precision)
  //      * use SegmentIdentificationSequence>ReferencedSegmentNumber to know
  //         what is the value to assign at the given pixel
  //  2) multiple stacks
  //      * use FrameContentSequence > StackId and InStackPositionNumber in conjunction
  //         with ImagePositionPatient and ReferencedSegmentNumber

  // TODO: segment->getFrameData() is missing?

  typedef itk::ChangeInformationImageFilter<ImageType> ChangeInfoFilter;
  ChangeInfoFilter::Pointer changeInfo = ChangeInfoFilter::New();
  changeInfo->SetInput(segImage);
  changeInfo->SetOutputSpacing(spacing);
  changeInfo->SetOutputDirection(dir);
  changeInfo->ChangeSpacingOn();
  changeInfo->ChangeDirectionOn();

  typedef itk::ImageFileWriter<ImageType> WriterType;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(outputNRRDFileName);
  writer->SetInput(changeInfo->GetOutput());
  writer->Update();

  std::cout << "Input spacing: " << spacing << " Output spacing: " << changeInfo->GetOutput()->GetSpacing() << std::endl;

  return 0;
}
