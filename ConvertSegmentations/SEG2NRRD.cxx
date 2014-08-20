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

#define INCLUDE_CSTDLIB
#define INCLUDE_CSTRING
#include "dcmtk/ofstd/ofstdinc.h"

#include <sstream>

#ifdef WITH_ZLIB
#include <zlib.h>                     /* for zlibVersion() */
#endif

// ITK includes
#include <itkImageFileReader.h>
#include <itkLabelImageToLabelMapFilter.h>
#include <itkImageRegionConstIterator.h>

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

  ImageType::PointType orientX, orientY, origin;
  ImageType::RegionType imageRegion;
  ImageType::SizeType imageSize;

  {
    OFString str;
    if(segDataset->findAndGetOFString(DCM_Rows, str).good()){
      imageSize[0] = atoi(str.c_str());
    }
    if(segDataset->findAndGetOFString(DCM_Columns, str).good()){
      imageSize[1] = atoi(str.c_str());
    }
  }

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
      orientX[i] = atof(orientStr.c_str());
    }
  }
  for(int i=3;i<6;i++){
    if(planorfg->getImageOrientationPatient(orientStr, i).good()){
      orientY[i] = atof(orientStr.c_str());
    }
  }
  }

  // Size: Rows/Columns can be read directly from the respective attributes
  // For number of slices, consider that all segments must have the same number of frames.
  //   If we have FoR UID initialized, this means every segment should also have Plane
  //   Position (Patient) initialized. So we can get the number of slices by looking
  //   how many per-frame functional groups a segment has.

  std::vector<ImageType::PointType> sliceOriginPoints;
  for(int i=0;;i++){
    FGPlanePosPatient *planposfg = OFstatic_cast(FGPlanePosPatient*,
                                                    fgInterface.getPerFrame(i, DcmFGTypes::EFG_PLANEPOSPATIENT));
    if(!planposfg){
        break;
    }
    std::cout << "Found plane position for frame " << i << std::endl;

    ImageType::PointType sOrigin;
    for(int j=0;j<3;j++){
      OFString planposStr;
      if(planposfg->getImagePositionPatient(planposStr, j).good()){
        sOrigin[j] = atof(planposStr.c_str());
      } else {
        std::cerr << "Failed to read patient position" << std::endl;
      }
    }
    sliceOriginPoints.push_back(sOrigin);
    imageSize[2] = sliceOriginPoints.size();
  }

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

  return 0;
}
