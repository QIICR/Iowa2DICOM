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

double distanceBwPoints(vnl_vector<double> from, vnl_vector<double> to){
  return sqrt((from[0]-to[0])*(from[0]-to[0])+(from[1]-to[1])*(from[1]-to[1])+(from[2]-to[2])*(from[2]-to[2]));
}

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

  unsigned frameSize = inputSize[0]*inputSize[1];

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

  std::cout << "Binary segmentation created!" << std::endl;

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
#if 0
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
  #endif

  OFString imageOrientationPatientStr;
  // Shared FGs: PlaneOrientationPatientSequence
  {
    OFCondition cond;

    ImageType::DirectionType labelDirMatrix = labelImage->GetDirection();
    std::ostringstream orientationSStream;
    orientationSStream << std::scientific
                       << labelDirMatrix[0][0] << "\\" << labelDirMatrix[1][0] << "\\" << labelDirMatrix[2][0] << "\\"
                       << labelDirMatrix[0][1] << "\\" << labelDirMatrix[1][1] << "\\" << labelDirMatrix[2][1];
    imageOrientationPatientStr = orientationSStream.str().c_str();

    FGPlaneOrientationPatient *planor =
        FGPlaneOrientationPatient::createMinimal(imageOrientationPatientStr);
    cond = planor->setImageOrientationPatient(imageOrientationPatientStr);
    if(cond.good())
      std::cout << "Plane orientation inserted and initialzied" << std::endl;
    cond = segdoc->addForAllFrames(*planor);
  }

  // Shared FGs: PixelMeasuresSequence
  {
    OFCondition cond;

    FGPixelMeasures *pixmsr = new FGPixelMeasures();

    ImageType::SpacingType labelSpacing = labelImage->GetSpacing();
    std::ostringstream spacingSStream;
    spacingSStream << std::scientific << labelSpacing[0] << "\\" << labelSpacing[1];
    pixmsr->setPixelSpacing(spacingSStream.str().c_str());
    std::ostringstream spacingBetweenSlicesSStream;
    spacingBetweenSlicesSStream << std::scientific << labelSpacing[2];
    pixmsr->setSpacingBetweenSlices(spacingBetweenSlicesSStream.str().c_str());
    segdoc->addForAllFrames(*pixmsr);
  }

  FGPlanePosPatient* fgppp = FGPlanePosPatient::createMinimal("1\\1\\1");
  FGFrameContent* fgfc = new FGFrameContent();
  OFVector<FGBase*> perFrameFGs;
  perFrameFGs.push_back(fgppp);
  perFrameFGs.push_back(fgfc);

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

      if(!label){
        std::cout << "Skipping label 0" << std::endl;
        continue;
      }

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
        std::cout << "Segment " << segmentNumber << " created" << std::endl;
      }

      // TODO: make it possible to skip empty frames (optional)
      // iterate over slices for an individual label and populate output frames      
      for(int sliceNumber=0;sliceNumber<inputSize[2];sliceNumber++){

        // segments are numbered starting from 1
        Uint32 frameNumber = (segmentNumber-1)*inputSize[2]+sliceNumber;

        OFString imagePositionPatientStr;

        // PerFrame FG: FrameContentSequence
        //fracon->setStackID("1"); // all frames go into the same stack (?)
        fgfc->setDimensionIndexValues(segmentNumber, 0);
        fgfc->setDimensionIndexValues(sliceNumber+1, 1);
        //std::ostringstream inStackPosSStream; // StackID is not present/needed
        //inStackPosSStream << s+1;
        //fracon->setInStackPositionNumber(s+1);

        // PerFrame FG: PlanePositionSequence
        {         
          ImageType::PointType sliceOriginPoint;
          ImageType::IndexType sliceOriginIndex;
          sliceOriginIndex.Fill(0);
          sliceOriginIndex[2] = sliceNumber;
          labelImage->TransformIndexToPhysicalPoint(sliceOriginIndex, sliceOriginPoint);
          std::ostringstream pppSStream;
          if(sliceNumber>0){
            ImageType::PointType prevOrigin;
            ImageType::IndexType prevIndex;
            prevIndex.Fill(0);
            prevIndex[2] = sliceNumber-1;
            labelImage->TransformIndexToPhysicalPoint(prevIndex, prevOrigin);
          }
          pppSStream << std::scientific << sliceOriginPoint[0] << "\\" << sliceOriginPoint[1] << "\\" << sliceOriginPoint[2];
          imagePositionPatientStr = OFString(pppSStream.str().c_str());
          fgppp->setImagePositionPatient(imagePositionPatientStr);
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
          itk::ImageRegionConstIterator<ImageType> sliceIterator(labelImage, sliceRegion);
          for(sliceIterator.GoToBegin();!sliceIterator.IsAtEnd();++sliceIterator,++framePixelCnt){
            if(sliceIterator.Get() == label)
              frameData[framePixelCnt] = 1;
            else
              frameData[framePixelCnt] = 0;
          }
          OFVector<ImageSOPInstanceReferenceMacro> derivationImages;
          // derivation images list is optional
          derivationImages.clear();
          // FIXME: ImageOrientationPatient will be added per frame!
          result = segdoc->addFrame(frameData, segmentNumber, perFrameFGs);
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
  //segdoc->saveFile(outputSEGFileName.c_str(), EXS_LittleEndianExplicit);

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
