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
#include "dcmtk/dcmfg/fgseg.h"

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

  vnl_vector<double> dirX(3), dirY(3);
  vnl_vector<double> rowDirection(3), colDirection(3), sliceDirection(3);

  ImageType::PointType imageOrigin;
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

  // Orientation
  ImageType::DirectionType dir;
  {
    // For directions, we can only handle segments that have patient orientation
    //  identical for all frames, so either find it in shared FG, or fail
    // TODO: handle the situation when FoR is not initialized
    OFBool isPerFrame;
    FGPlaneOrientationPatient *planorfg = OFstatic_cast(FGPlaneOrientationPatient*,
                                                        fgInterface.get(0, DcmFGTypes::EFG_PLANEORIENTPATIENT, isPerFrame));
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
    sliceDirection = vnl_cross_3d(dirX, dirY);
    sliceDirection.normalize();

    colDirection = dirY;
    rowDirection = dirX;
    //rowDirection = vnl_cross_3d(colDirection, sliceDirection).normalize();

    //colDirection.normalize();

    for(int i=0;i<3;i++){
      dir[0][i] = rowDirection[i];
      dir[1][i] = colDirection[i];
      dir[2][i] = sliceDirection[i];
    }
  }

  // Size
  // Rows/Columns can be read directly from the respective attributes
  // For number of slices, consider that all segments must have the same number of frames.
  //   If we have FoR UID initialized, this means every segment should also have Plane
  //   Position (Patient) initialized. So we can get the number of slices by looking
  //   how many per-frame functional groups a segment has.

  std::vector<double> originDistances;
  std::map<OFString, double> originStr2distance;
  std::map<OFString, unsigned> frame2overlap;
  double minDistance;

  unsigned numFrames = 0, numSegments = 0;

  // Determine ordering of the frames, keep mapping from ImagePositionPatient string
  //   to the distance, and keep track (just out of curiousity) how many frames overlap
  for(int frameId=0;;frameId++,numFrames++){
    OFBool isPerFrame;
    FGPlanePosPatient *planposfg =
        OFstatic_cast(FGPlanePosPatient*,fgInterface.get(frameId, DcmFGTypes::EFG_PLANEPOSPATIENT, isPerFrame));

    if(!planposfg){
        break;
    }

    vnl_vector<double> sOrigin;
    OFString sOriginStr = "";
    sOrigin.set_size(3);
    for(int j=0;j<3;j++){
      OFString planposStr;
      if(planposfg->getImagePositionPatient(planposStr, j).good()){
          sOrigin[j] = atof(planposStr.c_str());
          sOriginStr += planposStr;
          if(j<2)
            sOriginStr+='/';
      } else {
        std::cerr << "Failed to read patient position" << std::endl;
      }
    }

    if(originStr2distance.find(sOriginStr) == originStr2distance.end()){
      double dist = dot_product(sliceDirection,sOrigin);
      frame2overlap[sOriginStr] = 1;
      originStr2distance[sOriginStr] = dist;
      originDistances.push_back(dist);
      if(frameId==0){
        minDistance = dist;
        imageOrigin[0] = sOrigin[0];
        imageOrigin[1] = sOrigin[1];
        imageOrigin[2] = sOrigin[2];
      }
      else
        if(dist<minDistance){
          imageOrigin[0] = sOrigin[0];
          imageOrigin[1] = sOrigin[1];
          imageOrigin[2] = sOrigin[2];
          minDistance = dist;
        }
    } else {
      frame2overlap[sOriginStr]++;
    }
    if(originStr2distance.find(sOriginStr) == originStr2distance.end())
      abort();
  }

  // sort all unique distances, this will be used to check consistency of
  //  slice spacing, and also to locate the slice position from ImagePositionPatient
  //  later when we read the segments
  // TODO: it seems like frames within the segment may not need to be consecutive,
  //  so inconsistent slice gap may not indicate a problem, but SliceThickness is not
  //  mandatory in the PixelMeasuresSequence. Need to discuss with David.
  sort(originDistances.begin(), originDistances.end());

  float dist0 = fabs(originDistances[0]-originDistances[1]);
  for(int i=1;i<originDistances.size();i++){
    float dist1 = fabs(originDistances[i-1]-originDistances[i]);
    float delta = dist0-dist1;
    if(delta > 0.001){
      std::cerr << "Inter-slice distance " << originDistances[i] << " difference exceeded threshold: " << delta << std::endl;
    }
  }

  float zDim = fabs(originDistances[0]-originDistances[originDistances.size()-1]);
  unsigned overlappingFramesCnt = 0;
  for(std::map<OFString, unsigned>::const_iterator it=frame2overlap.begin();
      it!=frame2overlap.end();++it){
    if(it->second>1)
      overlappingFramesCnt++;
  }

  std::cout << "Total frames: " << numFrames << std::endl;
  std::cout << "Total frames with unique IPP: " << originDistances.size() << std::endl;
  std::cout << "Total overlapping frames: " << overlappingFramesCnt << std::endl;

  std::sort(originDistances.begin(), originDistances.end());  

  std::cout << "Origin: " << imageOrigin << std::endl;

  OFBool isPerFrame;
  FGPixelMeasures *pixm = OFstatic_cast(FGPixelMeasures*,
                                                      fgInterface.get(0, DcmFGTypes::EFG_PIXELMEASURES, isPerFrame));
  if(!pixm){
    std::cerr << "Pixel spacing is missing, cannot parse input " << std::endl;
    return -1;
  }

  OFString spacingStr;
  pixm->getPixelSpacing(spacingStr, 0);
  spacing[0] = atof(spacingStr.c_str());
  pixm->getPixelSpacing(spacingStr, 1);
  spacing[1] = atof(spacingStr.c_str());
  pixm->getSliceThickness(spacingStr, 0);
  spacing[2] = atof(spacingStr.c_str());

  {
    double derivedSpacing = fabs(originDistances[0]-originDistances[1]);
    double eps = 0.0001;
    if(fabs(spacing[2]-derivedSpacing)>eps){
      std::cerr << "WARNING: PixelMeasures FG SliceThickness difference of "
                   << fabs(spacing[2]-derivedSpacing) << " exceeds threshold of "
                   << eps << std::endl;
    }
  }

  std::cout << "Spacing: " << spacing << std::endl;

  imageSize[2] = zDim/spacing[2]+1;

  // Initialize the image
  imageRegion.SetSize(imageSize);
  segImage->SetRegions(imageRegion);
  segImage->SetOrigin(imageOrigin);
  segImage->SetSpacing(spacing);
  segImage->SetDirection(dir);

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
  //      * use ImagePositionPatient to find the matching slice
  //      * use SegmentIdentificationSequence>ReferencedSegmentNumber to know
  //         what is the value to assign at the given pixel
  //  2) multiple stacks
  //      * use FrameContentSequence > StackId and InStackPositionNumber in conjunction
  //         with ImagePositionPatient and ReferencedSegmentNumber

  segImage->FillBuffer(0);

  std::vector<unsigned> segmentPixelCnt;

  for(int frameId=0;frameId<numFrames;frameId++){
    const DcmSegmentation::Frame *frame = segdoc->getFrame(frameId);

    FGPlanePosPatient *planposfg =
        OFstatic_cast(FGPlanePosPatient*,fgInterface.get(frameId, DcmFGTypes::EFG_PLANEPOSPATIENT, isPerFrame));
    assert(planposfg);

    FGFrameContent *fracon =
        OFstatic_cast(FGFrameContent*,fgInterface.get(frameId, DcmFGTypes::EFG_FRAMECONTENT, isPerFrame));
    assert(fracon);

    FGSegmentation *fgseg =
        OFstatic_cast(FGSegmentation*,fgInterface.get(frameId, DcmFGTypes::EFG_SEGMENTATION, isPerFrame));
    assert(fgseg);

    Uint16 segmentId = -1;
    if(fgseg->getReferencedSegmentNumber(segmentId).bad()){
      std::cerr << "Failed to get seg number!";
      abort();
    }

    // WARNING: this is needed only for David's example, which numbers
    // (incorrectly!) segments starting from 0, should start from 1
    segmentId = segmentId+1;

    if(segmentId>segmentPixelCnt.size())
      segmentPixelCnt.resize(segmentId, 0);

    // get string representation of the frame origin
    OFString sOriginStr;
    ImageType::PointType frameOriginPoint;
    ImageType::IndexType frameOriginIndex;
    for(int j=0;j<3;j++){
      OFString planposStr;
      if(planposfg->getImagePositionPatient(planposStr, j).good()){
        frameOriginPoint[j] = atof(planposStr.c_str());
      }
    }

    if(!segImage->TransformPhysicalPointToIndex(frameOriginPoint, frameOriginIndex)){
      std::cerr << "ERROR: Frame " << frameId << " origin " << frameOriginPoint <<
                   " is outside image geometry!" << frameOriginIndex << std::endl;
      std::cerr << segImage << std::endl;
      abort();
    }

    unsigned slice = frameOriginIndex[2];

    // initialize slice with the frame content
    for(int row=0;row<imageSize[1];row++){
      for(int col=0;col<imageSize[0];col++){
        ImageType::PixelType pixel;
        unsigned bitCnt = row*imageSize[0]+col;
        pixel = (frame->pixData[bitCnt/8] >> (bitCnt%8)) & 1;
        if(pixel!=0){
          segmentPixelCnt[segmentId-1]++;
          pixel = pixel+segmentId;
          ImageType::IndexType index;
          index[0] = col;
          index[1] = row;
          index[2] = slice;
          if(segImage->GetPixel(index))
            std::cout << "Warning: overwriting pixel at index " << index << std::endl;
          segImage->SetPixel(index, pixel);
        }
      }
    }
  }

  std::cout << "Number of pixels for segments: " << std::endl;
  for(unsigned i=1;i<segmentPixelCnt.size();i++)
    std::cout << i << ":" << segmentPixelCnt[i] << std::endl;

  typedef itk::ImageFileWriter<ImageType> WriterType;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(outputNRRDFileName);
  writer->SetInput(segImage);
  writer->Update();

  return 0;
}
