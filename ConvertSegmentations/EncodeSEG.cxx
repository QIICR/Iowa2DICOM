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

#include "dcmtk/dcmiod/iodmacro.h"

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

// UIDs
#include "../Common/QIICRUIDs.h"
//#include "../Common/conditionCheckMacros.h"

// versioning
#include "../Iowa2DICOMVersionConfigure.h"

// CLP inclides
#include "EncodeSEGCLP.h"

static OFLogger locallogger = OFLog::getLogger("qiicr.apps.iowa1");

#define CHECK_COND(condition) \
      do { \
                if (condition.bad()) { \
                              OFLOG_FATAL(locallogger, condition.text() << " in " __FILE__ << ":" << __LINE__ ); \
                              throw -1; \
                          } \
            } while (0)

double distanceBwPoints(vnl_vector<double> from, vnl_vector<double> to){
  return sqrt((from[0]-to[0])*(from[0]-to[0])+(from[1]-to[1])*(from[1]-to[1])+(from[2]-to[2])*(from[2]-to[2]));
}

int main(int argc, char *argv[])
{
  PARSE_ARGS;

  //dcemfinfLogger.setLogLevel(dcmtk::log4cplus::OFF_LOG_LEVEL);

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

  //OFLog::configure(OFLogger::DEBUG_LOG_LEVEL);
  
  /* Construct Equipment information */
  IODEquipmentModule::EquipmentInfo eq;
  eq.m_Manufacturer = "QIICR";
  eq.m_DeviceSerialNumber = "0";
  eq.m_ManufacturerModelName = Iowa2DICOM_WC_URL;
  eq.m_SoftwareVersions = Iowa2DICOM_WC_REVISION;

  /* Construct Content identification information */
  ContentIdentificationMacro ident;
  CHECK_COND(ident.setContentCreatorName("QIICR"));
  CHECK_COND(ident.setContentDescription("Iowa QIN segmentation result"));
  CHECK_COND(ident.setContentLabel("QINIOWA"));
  CHECK_COND(ident.setInstanceNumber("1234")); // is there a better way to initialize this?

  /* Create new segementation document */
  DcmDataset segdocDataset;
  DcmSegmentation *segdoc = NULL;

  CHECK_COND(DcmSegmentation::createBinarySegmentation(
    segdoc,   // resulting segmentation
    inputSize[0],      // rows
    inputSize[1],      // columns
    eq,       // equipment
    ident));   // content identification

  /* Import patient and study from existing file */
  CHECK_COND(segdoc->importPatientStudyFoR(inputDICOMImageFileNames[0].c_str(), OFTrue, OFTrue, OFFalse, OFTrue));

  /* Series Number is part 1 and we do not take over the series, so set it
   * TODO: Invent automatically if not set by user
   */
  CHECK_COND(segdoc->getSeries().setSeriesNumber("4711"));

  /* Initialize dimension module */
  char dimUID[128];
  dcmGenerateUniqueIdentifier(dimUID, QIICR_UID_ROOT);
  IODMultiframeDimensionModule &mfdim = segdoc->getDimensions();
  CHECK_COND(mfdim.addDimensionIndex(DCM_ReferencedSegmentNumber, dimUID, DCM_SegmentIdentificationSequence,
                             DcmTag(DCM_ReferencedSegmentNumber).getTagName()));
  CHECK_COND(mfdim.addDimensionIndex(DCM_ImagePositionPatient, dimUID, DCM_PlanePositionSequence,
                             DcmTag(DCM_ImagePositionPatient).getTagName()));

  /* Initialize shared functional groups */
  FGInterface &segFGInt = segdoc->getFunctionalGroups();

  // Find mapping from the segmentation slice number to the derivation image
  // Assume that orientation of the segmentation is the same as the source series
  std::vector<int> slice2derimg(inputDICOMImageFileNames.size());
  for(int i=0;i<inputDICOMImageFileNames.size();i++){
    OFString ippStr;
    DcmFileFormat sliceFF;
    DcmDataset *sliceDataset = NULL;
    ImageType::PointType ippPoint;
    ImageType::IndexType ippIndex;
    CHECK_COND(sliceFF.loadFile(inputDICOMImageFileNames[i].c_str()));
    sliceDataset = sliceFF.getDataset();
    for(int j=0;j<3;j++){
      CHECK_COND(sliceDataset->findAndGetOFString(DCM_ImagePositionPatient, ippStr, j));
      ippPoint[j] = atof(ippStr.c_str());
    }
    if(!labelImage->TransformPhysicalPointToIndex(ippPoint, ippIndex)){
      std::cerr << "ImagePositionPatient maps outside the ITK image!" << std::endl;
      return -1;
    }
    slice2derimg[ippIndex[2]] = i;
  }

  // Shared FGs: PlaneOrientationPatientSequence
  {
    OFString imageOrientationPatientStr;

    ImageType::DirectionType labelDirMatrix = labelImage->GetDirection();
    std::ostringstream orientationSStream;
    orientationSStream << std::scientific
                       << labelDirMatrix[0][0] << "\\" << labelDirMatrix[1][0] << "\\" << labelDirMatrix[2][0] << "\\"
                       << labelDirMatrix[0][1] << "\\" << labelDirMatrix[1][1] << "\\" << labelDirMatrix[2][1];
    imageOrientationPatientStr = orientationSStream.str().c_str();

    FGPlaneOrientationPatient *planor =
        FGPlaneOrientationPatient::createMinimal(imageOrientationPatientStr);
    CHECK_COND(planor->setImageOrientationPatient(imageOrientationPatientStr));
    CHECK_COND(segdoc->addForAllFrames(*planor));
  }

  // Shared FGs: PixelMeasuresSequence
  {
    FGPixelMeasures *pixmsr = new FGPixelMeasures();

    ImageType::SpacingType labelSpacing = labelImage->GetSpacing();
    std::ostringstream spacingSStream;
    spacingSStream << std::scientific << labelSpacing[0] << "\\" << labelSpacing[1];
    CHECK_COND(pixmsr->setPixelSpacing(spacingSStream.str().c_str()));
    std::ostringstream spacingBetweenSlicesSStream;
    spacingBetweenSlicesSStream << std::scientific << labelSpacing[2];
    CHECK_COND(pixmsr->setSpacingBetweenSlices(spacingBetweenSlicesSStream.str().c_str()));
    CHECK_COND(segdoc->addForAllFrames(*pixmsr));
  }

  FGPlanePosPatient* fgppp = FGPlanePosPatient::createMinimal("1\\1\\1");
  FGFrameContent* fgfc = new FGFrameContent();
  FGDerivationImage* fgder = new FGDerivationImage();
  OFVector<FGBase*> perFrameFGs;

  perFrameFGs.push_back(fgppp);
  perFrameFGs.push_back(fgfc);
  perFrameFGs.push_back(fgder);

  // Iterate over the files and labels available in each file, create a segment for each label,
  //  initialize segment frames and add to the document

  OFString seriesInstanceUID, classUID;
  std::vector<OFString> instanceUIDs;

  IODCommonInstanceReferenceModule &commref = segdoc->getCommonInstanceReference();
  OFVector<IODSeriesAndInstanceReferenceMacro::ReferencedSeriesItem*> &refseries = commref.getReferencedSeriesItems();

  IODSeriesAndInstanceReferenceMacro::ReferencedSeriesItem refseriesItem;
  refseries.push_back(&refseriesItem);

  OFVector<SOPInstanceReferenceMacro*> &refinstances = refseriesItem.getReferencedInstanceItems();

  DcmFileFormat ff;
  CHECK_COND(ff.loadFile(inputDICOMImageFileNames[slice2derimg[0]].c_str()));
  DcmDataset *dcm = ff.getDataset();
  CHECK_COND(dcm->findAndGetOFString(DCM_SeriesInstanceUID, seriesInstanceUID));
  CHECK_COND(refseriesItem.setSeriesInstanceUID(seriesInstanceUID));


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

      CHECK_COND(DcmSegment::create(segment,
                                segmentNameStream.str().c_str(),
                                CodeSequenceMacro("T-D000A", "SRT", "Anatomical Structure"),
                                CodeSequenceMacro("T-A6000", "SRT", "Cerebellum"),
                                DcmSegTypes::SAT_MANUAL,
                                ""));

      Uint16 segmentNumber;
      CHECK_COND(segdoc->addSegment(segment, segmentNumber /* returns logical segment number */));

      // TODO: make it possible to skip empty frames (optional)
      // iterate over slices for an individual label and populate output frames      
      for(int sliceNumber=0;sliceNumber<inputSize[2];sliceNumber++){

        // segments are numbered starting from 1
        Uint32 frameNumber = (segmentNumber-1)*inputSize[2]+sliceNumber;

        OFString imagePositionPatientStr;

        // PerFrame FG: FrameContentSequence
        //fracon->setStackID("1"); // all frames go into the same stack (?)
        CHECK_COND(fgfc->setDimensionIndexValues(segmentNumber, 0));
        CHECK_COND(fgfc->setDimensionIndexValues(sliceNumber+1, 1));
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
          fgder->clearData();

          DerivationImageItem *derimgItem;
          CHECK_COND(fgder->addDerivationImageItem(CodeSequenceMacro("113076","DCM","Segmentation"),"",derimgItem));
          std::cout << "Num der images: " << fgder->getDerivationImageItems().size() << std::endl;

          OFVector<OFString> siVector;
          siVector.push_back(OFString(inputDICOMImageFileNames[slice2derimg[sliceNumber]].c_str()));
          std::cout << " derivation image file name: " << inputDICOMImageFileNames[slice2derimg[sliceNumber]] << std::endl;
          SourceImageItem* srcimgItem;
          CHECK_COND(derimgItem->addSourceImageItem(siVector,
              CodeSequenceMacro("121322","DCM","Source image for image processing operation"),
              srcimgItem));

          CHECK_COND(segdoc->addFrame(frameData, segmentNumber, perFrameFGs));
          std::cout << "Added tis many derimg: " << fgder->getDerivationImageItems().size() << std::endl;

          if(1){
            // initialize class UID and series instance UID
            ImageSOPInstanceReferenceMacro &instRef = srcimgItem->getImageSOPInstanceReference();
            OFString instanceUID;
            CHECK_COND(instRef.getSOPClassUID(classUID));
            CHECK_COND(instRef.getSOPInstanceUID(instanceUID));
            instanceUIDs.push_back(instanceUID);

            SOPInstanceReferenceMacro *refinstancesItem = new SOPInstanceReferenceMacro();
            CHECK_COND(refinstancesItem->setSOPClassUID(classUID));
            CHECK_COND(refinstancesItem->setSOPInstanceUID(instanceUID));
            refinstances.push_back(refinstancesItem);
          }
        }
      }
    }
  }

  COUT << "Successfully created segmentation document" << OFendl;

  /* Store to disk */
  COUT << "Saving the result to " << outputSEGFileName << OFendl;
  //segdoc->saveFile(outputSEGFileName.c_str(), EXS_LittleEndianExplicit);

  CHECK_COND(segdoc->writeDataset(segdocDataset));

  DcmFileFormat segdocFF(&segdocDataset);
  CHECK_COND(segdocFF.saveFile(outputSEGFileName.c_str(), EXS_LittleEndianExplicit));
  COUT << "Saved segmentation as " << outputSEGFileName << std::endl;

  return 0;
}
