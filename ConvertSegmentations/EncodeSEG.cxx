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
#include "EncodeSEGCLP.h"

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

  dcemfinfLogger.setLogLevel(dcmtk::log4cplus::OFF_LOG_LEVEL);

  typedef short PixelType;
  typedef itk::Image<PixelType,3> ImageType;
  typedef itk::ImageFileReader<ImageType> ReaderType;
  typedef itk::LabelImageToLabelMapFilter<ImageType> LabelToLabelMapFilterType;
  
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(inputSegmentationsFileNames[0].c_str());
  reader->Update();
  ImageType::Pointer labelImage = reader->GetOutput();

  ImageType::SizeType inputSize = labelImage->GetBufferedRegion().GetSize();
  std::cout << "Input image size: " << inputSize << std::endl;

  unsigned frameSize = inputSize[0]*inputSize[1]/8;

  OFLog::configure(OFLogger::DEBUG_LOG_LEVEL);
  
  /* Construct Equipment information */
  IODEquipmentModule::EquipmentInfo eq;
  eq.m_Manufacturer = "QIICR";
  eq.m_DeviceSerialNumber = "1234";
  eq.m_ManufacturerModelName = "TEST DEVICE";
  eq.m_SoftwareVersions = "My own alpha version";

  /* Construct Content identification information */
  ContentIdentificationMacro ident;
  ident.setContentCreatorName("ANDREY");
  ident.setContentDescription("TEST SEGMENTATION CONTENT");
  ident.setContentLabel("TEST LABEL");
  ident.setInstanceNumber("1234");

  /* Create new segementation document */
  DcmDataset segdocDataset;
  DcmSegmentation *segdoc = NULL;

  OFCondition result = DcmSegmentation::createBinarySegmentation(
    segdoc,   // resulting segmentation
    inputSize[0],      // rows
    inputSize[1],      // columns
    eq,       // equipment
    ident);   // content identification
  if ( result.bad() )
  {
    CERR << "Could not create Segmentation document: " << result.text() << OFendl;
  }

  /* Import patient and study from existing file */
  result = segdoc->importPatientStudyFoR(inputDICOMImageFileNames[0].c_str(), OFTrue, OFTrue, OFFalse, OFTrue);
  if ( result.bad() )
  {
    CERR << "Warning: Could not import patient, study, series and/or frame of reference" << OFendl;
    result = EC_Normal;
  }

  /* Series Number is part 1 and we do not take over the series, so set it
   * TODO: Invent automatically if not set by user
   */
  segdoc->getSeries().setSeriesNumber("4711");


  char dimUID[128];
  dcmGenerateUniqueIdentifier(dimUID, SITE_UID_ROOT);
  IODMultiframeDimensionModule &mfdim = segdoc->getDimensions();
  mfdim.addDimensionIndex(DCM_SegmentNumber, dimUID, DCM_SegmentIdentificationSequence,
                             DcmTag(DCM_SegmentNumber).getTagName());
  mfdim.addDimensionIndex(DCM_ImagePositionPatient, dimUID, DCM_PlanePositionSequence,
                             DcmTag(DCM_ImagePositionPatient).getTagName());

  FGInterface &segFGInt = segdoc->getFunctionalGroups();

  // DerivationImageSequence and Common Instance Reference module: optional, since
  // we have FrameOfReferenceUID, and will specify PixelMeasures, PlanePosition and PlaneOrientation
  if(0){
    FGBase* existing = segFGInt.getShared(DcmFGTypes::EFG_DERIVATIONIMAGE);
    FGDerivationImage *derimg = NULL;
    OFCondition cond;

    if(!existing){
      derimg = new FGDerivationImage();
      cond = segFGInt.insertShared(derimg, OFTrue);
      if(cond.good()){
        DerivationImageItem *derImgItem;
        cond = derimg->addDerivationImageItem(CodeSequenceMacro("113076","DCM","Segmentation"),"",derImgItem);
        if(cond.good()){
          OFVector<OFString> siVector;
          for(int i=0;i<inputDICOMImageFileNames.size();i++){
            siVector.push_back(OFString(inputDICOMImageFileNames[i].c_str()));
          }
          SourceImageItem *srcImageItem = NULL;
          cond = derImgItem->addSourceImageItem(siVector,
              CodeSequenceMacro("121322","DCM","Source image for image processing operation"),
              srcImageItem);
        }
      }
      if(cond.good())
        std::cout << "Derivation image inserted and initialized" << std::endl;
    } else {
      std::cout << "Derivation image already exists" << std::endl;
    }
  }

  // Shared FGs: PlaneOrientationPatientSequence
  {
    FGBase* existing = segFGInt.getShared(DcmFGTypes::EFG_PLANEORIENTPATIENT);
    FGPlaneOrientationPatient *planor = NULL;
    OFCondition cond;

    if(!existing){
      planor = new FGPlaneOrientationPatient();
      cond = segFGInt.insertShared(planor, OFTrue);
      if(cond.good()){
        ImageType::DirectionType labelDirMatrix = labelImage->GetDirection();
        std::ostringstream orientationSStream;
        orientationSStream << labelDirMatrix[0][0] << "\\" << labelDirMatrix[1][0] << "\\" << labelDirMatrix[2][0] << "\\"
                           << labelDirMatrix[0][1] << "\\" << labelDirMatrix[1][1] << "\\" << labelDirMatrix[2][1];
        cond = planor->setImageOrientationPatient(orientationSStream.str().c_str());
      }
      if(cond.good())
        std::cout << "Plane orientation inserted and initialzied" << std::endl;
    } else {
      std::cout << "Plane orientation already exists" << std::endl;
    }

  }

  // Shared FGs: PixelMeasuresSequence
  {
    FGBase* existing = segFGInt.getShared(DcmFGTypes::EFG_PIXELMEASURES);
    FGPixelMeasures *pixmsr = NULL;
    OFCondition cond;

    if(!existing){
      pixmsr = new FGPixelMeasures();
      cond = segFGInt.insertShared(pixmsr, OFTrue);
      if(cond.good()){
        ImageType::SpacingType labelSpacing = labelImage->GetSpacing();
        std::ostringstream spacingSStream;
        spacingSStream << labelSpacing[0] << "\\" << labelSpacing[1];
        cond = pixmsr->setPixelSpacing(spacingSStream.str().c_str());
        std::ostringstream sliceThicknessSStream;
        sliceThicknessSStream << labelSpacing[2];
        cond = pixmsr->setSliceThickness(sliceThicknessSStream.str().c_str());
      }
      if(cond.good()){
        std::cout << "Pixel measures inserted and initialized" << std::endl;
      }
    }
  }

  // Iterate over the files and labels available in each file, create a segment for each label,
  //  initialize segment frames and add to the document

  Uint8 frameData[frameSize];
  for(int segFileNumber=0;segFileNumber<inputSegmentationsFileNames.size();segFileNumber++){
    std::cout << "Processing input label " << inputSegmentationsFileNames[segFileNumber] << std::endl;
    LabelToLabelMapFilterType::Pointer l2lm = LabelToLabelMapFilterType::New();
    ImageType::Pointer labelImage = reader->GetOutput();

    l2lm->SetInput(labelImage);
    l2lm->Update();

    typedef LabelToLabelMapFilterType::OutputImageType::LabelObjectType LabelType;
    std::cout << "Found " << l2lm->GetOutput()->GetNumberOfLabelObjects() << " label(s)" << std::endl;

    for(int segLabelNumber=0;segLabelNumber<l2lm->GetOutput()->GetNumberOfLabelObjects();segLabelNumber++){
      LabelType* labelObject = l2lm->GetOutput()->GetNthLabelObject(segLabelNumber);
      short label = labelObject->GetLabel();

      if(!label)
        continue;

      DcmSegment* segment = NULL;

      std::stringstream segmentNameStream;
      segmentNameStream << inputSegmentationsFileNames[segFileNumber] << " label " << label;

      result = DcmSegment::create(segment,
                                segmentNameStream.str().c_str(),
                                CodeSequenceMacro("TestCode", "99DCMTK", "Test Property Type Meaning", "Test Property Type Version"),
                                CodeSequenceMacro("TestCode", "99DCMTK", "Test Category Type Meaning", "Test Category Type Version"),
                                DcmSegTypes::SAT_MANUAL,
                                "My own Algorithm");

      Uint16 segmentNumber;
      if ( result.good() )
      {
        result = segdoc->addSegment(segment, segmentNumber /* returns logical segment number */);
      }

      // iterate over slices for an individual label and populate output frames
      for(int sliceNumber=0;sliceNumber<inputSize[2];sliceNumber++){

        // PerFrame FG: FrameContentSequence
        {
          FGBase* existing = segFGInt.getPerFrame(sliceNumber, DcmFGTypes::EFG_FRAMECONTENT);
          FGFrameContent *fracon = NULL;
          if(!existing){
            fracon = new FGFrameContent();
            segFGInt.insertPerFrame(sliceNumber, fracon);
          } else {
            fracon = OFstatic_cast(FGFrameContent*, existing);
          }
          //fracon->setStackID("1"); // all frames go into the same stack (?)
          fracon->setDimensionIndexValues(segmentNumber, 0);
          fracon->setDimensionIndexValues(sliceNumber+1, 1);
          //std::ostringstream inStackPosSStream; // StackID is not present/needed
          //inStackPosSStream << s+1;
          //fracon->setInStackPositionNumber(s+1);
        }

        // PerFrame FG: PlanePositionSequence
        {
          Uint32 frameNumber = segmentNumber*inputSize[2]+sliceNumber;
          FGBase* existing = segFGInt.getPerFrame(frameNumber, DcmFGTypes::EFG_PLANEPOSPATIENT);
          FGPlanePosPatient *ppos = NULL;
          if(!existing){
            ppos = new FGPlanePosPatient();
            segFGInt.insertPerFrame(frameNumber, ppos);
          } else {
            ppos = OFstatic_cast(FGPlanePosPatient*, existing);
          }

          ImageType::PointType sliceOriginPoint;
          ImageType::IndexType sliceOriginIndex;
          sliceOriginIndex.Fill(0);
          sliceOriginIndex[2] = sliceNumber;
          labelImage->TransformIndexToPhysicalPoint(sliceOriginIndex, sliceOriginPoint);
          std::ostringstream pppSStream;
          pppSStream << sliceOriginPoint[0] << "\\" << sliceOriginPoint[1] << "\\" << sliceOriginPoint[2];
          ppos->setPlanePositionPatient(pppSStream.str().c_str(), OFTrue);
        }

        /* Add frame that references this segment */
        if ( result.good() )
        {
          ImageType::RegionType sliceRegion;
          ImageType::IndexType sliceIndex;
          ImageType::SizeType sliceSize;

          sliceIndex[0] = 0;
          sliceIndex[1] = 0;
          sliceIndex[2] = sliceNumber;

          sliceSize[0] = inputSize[0];
          sliceSize[1] = inputSize[1];
          sliceSize[2] = 1;

          sliceRegion.SetIndex(sliceIndex);
          sliceRegion.SetSize(sliceSize);

          unsigned framePixelCnt = 0;
          bzero(frameData, frameSize*sizeof(Uint8));
          itk::ImageRegionConstIterator<ImageType> sliceIterator(labelImage, sliceRegion);
          for(sliceIterator.GoToBegin();!sliceIterator.IsAtEnd();++sliceIterator,++framePixelCnt){
            PixelType pixelValue = sliceIterator.Get();
            unsigned int byte = framePixelCnt/8, bit = framePixelCnt%8;
            frameData[byte] |= (pixelValue == segLabelNumber ? 1:0) << bit;
          }
          result = segdoc->addFrame(frameData, segmentNumber);
        }
        if ( result.bad() )
        {
          CERR << "Failed to add a frame to the document: " << result.text() << OFendl;
        }
      }
    }
  }

  COUT << "Successfully created segmentation document" << OFendl;

  /* Store to disk */
  COUT << "Saving the result to " << outputSEGFileName << OFendl;
  if(segdoc->writeDataset(segdocDataset).good()){
    std::cout << "Wrote dataset" << std::endl;
  }

  DcmFileFormat segdocFF(&segdocDataset);
  result = segdocFF.saveFile(outputSEGFileName.c_str(), EXS_LittleEndianExplicit);
  if (result.bad())
  {
    CERR << "Could not save segmentation document to file: " << result.text() << OFendl;
  }

  return 0;
}
