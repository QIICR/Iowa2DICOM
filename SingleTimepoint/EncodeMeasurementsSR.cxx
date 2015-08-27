///// STL includes
#include <iostream>
#include <string>
#include <vector>

#include "../Common/QIICRUIDs.h"
#include "../Common/SegmentAttributes.h"

// DCMTK includes
#include "dcmtk/config/osconfig.h"    /* make sure OS specific configuration is included first */

#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmsr/dsrdoc.h"
#include "dcmtk/dcmdata/dcuid.h"
#include "dcmtk/dcmdata/dcfilefo.h"
#include "dcmtk/dcmsr/dsriodcc.h"
#include "dcmtk/dcmiod/modhelp.h"
//#include "dcmtk/dcmdata/modhelp.h"

#include "dcmtk/ofstd/oftest.h"

#include "dcmtk/dcmsr/dsrdoctr.h"
#include "dcmtk/dcmsr/dsrcontn.h"
#include "dcmtk/dcmsr/dsrnumtn.h"
#include "dcmtk/dcmsr/dsruidtn.h"
#include "dcmtk/dcmsr/dsrtextn.h"
#include "dcmtk/dcmsr/dsrcodtn.h"
#include "dcmtk/dcmsr/dsrimgtn.h"
#include "dcmtk/dcmsr/dsrcomtn.h"
#include "dcmtk/dcmsr/dsrpnmtn.h"

#include <stdlib.h>

#include "../Iowa2DICOMVersionConfigure.h"
#include "../Common/SegmentAttributes.h"

#include "EncodeMeasurementsSRCLP.h"

static OFLogger logger = OFLog::getLogger("qiicr.apps.iowa1");

/** Check if a condition is true.
 *  @param condition condition to check
 */
#define CHECK_COND(condition) \
    do { \
        if (condition.bad()) { \
            OFLOG_FATAL(logger, condition.text() << " in " __FILE__ << ":" << __LINE__ ); \
            throw -1; \
        } \
    } while (0)

/** Check if two values are equal. Can only be used inside OFTEST(). Both
 *  arguments must be compatible with OFOStringStream's operator<<.
 *  @param val1 first value to compare
 *  @param val2 second value to compare
 */
#define CHECK_EQUAL(val1, val2) \
    do { \
        if ((val1) != (val2)) { \
            OFLOG_FATAL(logger, "(" << (val1) << ") should equal (" << (val2) << ") in " << __FILE__ << ":" << __LINE__ ); \
            throw -2; \
        } \
    } while (0)

/** Check if return value is non-zero.
*     */
#define CHECK_NONZERO(ret) \
  do { \
    if (!(ret)) { \
      OFLOG_FATAL(logger, "(" << (ret) << ") should be non-zero in " << __FILE__ << ":" << __LINE__ ); \
      throw -2; \
    } \
  } while (0)

#define WARN_IF_ERROR(FunctionCall,Message) if(!FunctionCall) std::cout << "Return value is 0 for " << Message << std::endl;

int getReferencedInstances(DcmDataset* dataset,
                            std::vector<std::string> &classUIDs,
                            std::vector<std::string> &instanceUIDs);

struct QuantityUnitsPairType {
  DSRCodedEntryValue QuantityCode;
  DSRCodedEntryValue UnitsCode;
  bool Derivation;
};

#include "../Common/SegmentAttributes.h"
typedef std::map<std::string, QuantityUnitsPairType> QuantitiesDictionaryType;

struct ROIMeasurementType {
  DSRCodedEntryValue QuantityCode;
  DSRCodedEntryValue UnitsCode;
  std::string MeasurementValue;
  bool Derivation;
};

// coded pairs of quantity/units
typedef std::vector<ROIMeasurementType> Measurements;
typedef std::map<DSRCodedEntryValue, Measurements> StructureToMeasurementsType;


void InitializeRootNode(DSRDocumentTree&);

void AddLanguageOfContent(DSRDocumentTree&);
//void AddObservationContext(DSRDocumentTree&);
void AddDeviceObserverContext(DSRDocumentTree&,
                        const char* deviceObserverUID,
                        const char* deviceObserverName,
                        const char* deviceObserverManufacturer,
                        const char* deviceObserverModelName,
                        const char* deviceObserverSerialNumber);

void AddPersonObserverContext(DSRDocumentTree&,
                        const char* personObserverName);

void AddImageLibrary(DSRDocumentTree&, std::vector<DcmDataset*>&);
void AddImageLibraryEntryDescriptors(DSRDocumentTree&, DcmDataset*);
void AddImageLibraryEntry(DSRDocumentTree&, DcmDataset*);

OFCondition AddImageLibraryDescriptorFromTag(DSRDocumentTree&, DcmDataset*, const DcmTagKey,
    DSRTypes::E_RelationshipType,
    DSRTypes::E_ValueType,
    DSRTypes::E_AddMode,
    const DSRCodedEntryValue);

void PopulateMeasurementsGroup(DSRDocumentTree&, DSRContainerTreeNode*, DSRCodedEntryValue&, Measurements&,
                               std::vector<DcmDataset*> petDatasets,
                               DcmDataset* rwvmDataset,
                               DcmDataset* segDataset,
                               const char* timePointId,
                               const char* sessionId,
                               unsigned segmentNumber,
                               const char* trackingID,
                               const char* trackingUID,
                               DSRCodedEntryValue&,
                               DSRCodedEntryValue&
                               );

void ReadMeasurementsForStructure(std::string filename, Measurements &measurementsList, QuantitiesDictionaryType&);
void ReadMeasurements(std::string filename, std::vector<Measurements> &segmentMeasurements, QuantitiesDictionaryType &dict);
void MapMeasurementsCodeToQuantityAndUnits(std::string &code, DSRCodedEntryValue&, DSRCodedEntryValue&);
void MapFileNameToStructureCode(std::string filename, DSRCodedEntryValue&);

void TokenizeString(std::string,std::vector<std::string>&, std::string delimiter=" ");

void ReadQuantitiesDictionary(std::string filename, QuantitiesDictionaryType&);
void AddCodingScheme(DSRDocument *, const char* id, const char* uidRoot,
                     const char* name, const char* org = NULL);

int main(int argc, char** argv)
{
  PARSE_ARGS;

  /*
  // First read the measurements that we will be encoding
  StructureToMeasurementsType structureMeasurements;
  for(unsigned i=0;i<measurementsFileNames.size();i++){
    // for now, assign the codes manually, but when SEGs are ready, get them from DICOM
    Measurements measurementsList;
    DSRCodedEntryValue structureCode;
    ReadMeasurementsForStructure(measurementsList, measurementsFileNames[i]);
    MapFileNameToStructureCode(measurementsFileNames[i], structureCode);
    structureMeasurements[structureCode] = measurementsList;
  }
  */

  std::vector<DcmDataset*> petDatasets;
  DcmDataset* rwvmDataset;
  DcmDataset* segDataset;

  QuantitiesDictionaryType quantitiesDictionary;

  {
    // read input datasets
    DcmFileFormat ff;
    for(int i=0;i<inputPETFileNames.size();i++){
      CHECK_COND(ff.loadFile(inputPETFileNames[i].c_str()));
      petDatasets.push_back(ff.getAndRemoveDataset());
    }

    CHECK_COND(ff.loadFile(inputRWVMFileName.c_str()));
    rwvmDataset = ff.getAndRemoveDataset();

    CHECK_COND(ff.loadFile(inputSEGFileName.c_str()));
    segDataset = ff.getAndRemoveDataset();

    std::cout << "Reading dictionary from " << quantityTermsFileName << std::endl;
    ReadQuantitiesDictionary(quantityTermsFileName, quantitiesDictionary);
  }

  DSRDocument *doc = new DSRDocument();

  // create root document
  doc->createNewDocument(DSRTypes::DT_ComprehensiveSR);
  doc->setSeriesDescription(seriesDescription.c_str());
  doc->setSeriesNumber(seriesNumber.c_str());
  doc->setInstanceNumber(instanceNumber.c_str());

  DSRDocumentTree &tree = doc->getTree();

  InitializeRootNode(tree);

  AddLanguageOfContent(tree);

  // TODO: initialize to more meaningful values
  if(deviceUID!=""){
    AddDeviceObserverContext(tree, deviceUID.c_str(), deviceName == "" ? NULL : deviceName.c_str(),
        NULL, NULL, NULL);
  } else if(readerId!="") {
    AddPersonObserverContext(tree, readerId.c_str());
  } else {
    std::cerr << "ERROR: Either DeviceUID or Reader ID should be specified!" << std::endl;
    return -1;
  }

  {
    DSRCodeTreeNode *procedureCode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
    CHECK_COND(procedureCode->setConceptName(DSRCodedEntryValue("121058","DCM","Procedure reported")));
    CHECK_COND(procedureCode->setCode("44139-4","LN","PET whole body"));
    CHECK_COND(tree.addContentItem(procedureCode, DSRTypes::AM_afterCurrent));
  }

  AddImageLibrary(tree, petDatasets);

  for(int i=0;i<petDatasets.size();i++){
    doc->getCurrentRequestedProcedureEvidence().addItem(*petDatasets[i]);
  }
  doc->getCurrentRequestedProcedureEvidence().addItem(*rwvmDataset);
  doc->getCurrentRequestedProcedureEvidence().addItem(*segDataset);

  // Encode measurements
  {
    DSRContainerTreeNode *measurementsContainer = new DSRContainerTreeNode(DSRTypes::RT_contains);
    CHECK_COND(measurementsContainer->setConceptName(DSRCodedEntryValue("126010","DCM","Imaging Measurements")));
    CHECK_COND(tree.addContentItem(measurementsContainer, DSRTypes::AM_afterCurrent));

    // read meta-information for the segmentation file
    std::vector<SegmentAttributes> segmentAttributes;
    {
      unsigned segmentNumber = 1;
      std::ifstream attrStream(inputLabelAttributesFileName.c_str());
      while(!attrStream.eof()){
        std::string attrString;
        getline(attrStream,attrString);
        if(!attrString.size())
          break;
        std::string labelStr, attributesStr;
        SegmentAttributes segAttr(segmentNumber);
        SplitString(attrString,labelStr,attributesStr,";");
        unsigned labelId = atoi(labelStr.c_str());
        segAttr.populateAttributesFromString(attributesStr);
        segmentAttributes.push_back(segAttr);
        //label2attributes[labelId].PrintSelf();
      }
    }

    // initialize quantities dictionary using first line in the csv file
    std::vector<Measurements> segmentMeasurements(segmentAttributes.size());
    ReadMeasurements(measurementsFileName, segmentMeasurements, quantitiesDictionary);

    std::cout << "segmentMeasurements has " << segmentMeasurements.size() << " items" << std::endl;

    // create a volumetric ROI measurements container (TID 1411) for each ROI, with each measurement container
    //  keeping all of the measurements for that ROI
    for(unsigned i=0;i<segmentMeasurements.size();i++){
      // for now, assign the codes manually, but when SEGs are ready, get them from DICOM
      DSRCodedEntryValue anatomicalStructureCode;

      DSRContainerTreeNode *measurementsGroup = new DSRContainerTreeNode(DSRTypes::RT_contains);
      CHECK_COND(measurementsGroup->setConceptName(DSRCodedEntryValue("125007","DCM","Measurement Group")));
      CHECK_COND(measurementsGroup->setTemplateIdentification("1411","DCMR"));
      CHECK_COND(tree.addContentItem(measurementsGroup, DSRTypes::AM_belowCurrent));

      // pass segment number here as well - assign it to be the same as the
      // line number in the measurements file
      // TODO: add encoding of the Tracking UID!
      std::string trackingIDStr, trackingUIDStr;
      if(segmentAttributes[i].lookupAttribute("TrackingID") == ""){
        std::stringstream trackingIDStream;
        // Warning: specific to Iowa dataset, to avoid errors
        std::cerr << "ERROR: TrackingID is missing in the input!" << std::endl;
        abort();
        trackingIDStream << "Segment" << i+1;
        trackingIDStr = trackingIDStream.str();
      } else {
        trackingIDStr = segmentAttributes[i].lookupAttribute("TrackingID");
      }

      if(segmentAttributes[i].lookupAttribute("TrackingUID") == ""){
        char trackingUID[128];
        // Warning: specific to Iowa dataset, to avoid errors
        std::cerr << "ERROR: TrackingUID is missing in the input!" << std::endl;
        abort();
        dcmGenerateUniqueIdentifier(trackingUID, SITE_INSTANCE_UID_ROOT);
        trackingUIDStr = std::string(trackingUID);
      } else {
        trackingUIDStr = segmentAttributes[i].lookupAttribute("TrackingUID");
      }

      DSRCodedEntryValue findingCode("0","0","0"), findingSiteCode("0","0","0");
      std::string findingStr = segmentAttributes[i].lookupAttribute("SegmentedPropertyType"),
        findingSiteStr = segmentAttributes[i].lookupAttribute("AnatomicRegion");
      std::cout << findingStr << " " << findingSiteStr << std::endl;
      if(findingStr != "")
        findingCode = StringToDSRCodedEntryValue(findingStr);
      if(findingSiteStr != "")
        findingSiteCode = StringToDSRCodedEntryValue(findingSiteStr);

      PopulateMeasurementsGroup(tree, measurementsGroup, anatomicalStructureCode, segmentMeasurements[i],
                                petDatasets, rwvmDataset, segDataset, timePointId.c_str(), sessionId.c_str(), i+1,
                                trackingIDStr.c_str(), trackingUIDStr.c_str(),
                                findingCode, findingSiteCode);

      CHECK_EQUAL(tree.gotoNode(measurementsContainer->getNodeID()),measurementsContainer->getNodeID());
      //std::cout << "Encoding only one file!" << std::endl;
      //break;
    }

  }

  DcmFileFormat *fileformatSR = new DcmFileFormat();
  DcmDataset *datasetSR = fileformatSR->getDataset();

  OFString contentDate, contentTime;
  DcmDate::getCurrentDate(contentDate);
  DcmTime::getCurrentTime(contentTime);

  // Note: ContentDate/Time are populated by DSRDocument
  doc->setSeriesDate(contentDate.c_str());
  doc->setSeriesTime(contentTime.c_str());

  // software versioning
  doc->setManufacturerModelName(Iowa2DICOM_WC_URL);
  doc->setSoftwareVersions(Iowa2DICOM_WC_REVISION);

  AddCodingScheme(doc, "99PMP", "1.3.6.1.4.1.5962.98.1", "PixelMed Publishing");

  std::cout << "Before writing the dataset" << std::endl;
  doc->write(*datasetSR);

  DcmModuleHelpers::copyPatientModule(*petDatasets[0],*datasetSR);
  DcmModuleHelpers::copyPatientStudyModule(*petDatasets[0],*datasetSR);
  DcmModuleHelpers::copyGeneralStudyModule(*petDatasets[0],*datasetSR);

  if(outputSRFileName.size()){
    std::cout << "Will save the result to " << outputSRFileName << std::endl;
    fileformatSR->saveFile(outputSRFileName.c_str(), EXS_LittleEndianExplicit);
  }

  return 0;
}

void InitializeRootNode(DSRDocumentTree &tree){

  DSRContainerTreeNode *rootNode = new DSRContainerTreeNode(DSRTypes::RT_isRoot);
  CHECK_COND(tree.addContentItem(rootNode, DSRTypes::AM_afterCurrent));

  CHECK_COND(rootNode->setTemplateIdentification("1500", "DCMR"));
  rootNode->setConceptName(DSRCodedEntryValue("126000","DCM","Imaging Measurement Report"));
}


void AddLanguageOfContent(DSRDocumentTree &tree){

  DSRCodeTreeNode *langNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);

  langNode->setConceptName(DSRCodedEntryValue("121049", "DCM", "Language of Content Item and Descendants"));
  langNode->setCode("eng", "RFC3066", "English");

  CHECK_COND(tree.addContentItem(langNode, DSRTypes::AM_belowCurrent));

  DSRCodeTreeNode *countryNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
  CHECK_COND(countryNode->setConceptName(DSRCodedEntryValue("121046", "DCM", "Country of Language")));
  CHECK_COND(countryNode->setCode("US","ISO3166_1","United States"));

  CHECK_COND(tree.addContentItem(countryNode, DSRTypes::AM_belowCurrent));

  tree.goUp();
}

void AddDeviceObserverContext(DSRDocumentTree &tree,
                        const char* deviceObserverUID,
                        const char* deviceObserverName,
                        const char* deviceObserverManufacturer,
                        const char* deviceObserverModelName,
                        const char* deviceObserverSerialNumber){

    DSRCodeTreeNode *observerTypeNode = new DSRCodeTreeNode(DSRTypes::RT_hasObsContext);
    CHECK_COND(observerTypeNode->setConceptName(DSRCodedEntryValue("121005","DCM","Observer Type")));
    CHECK_COND(observerTypeNode->setCode("121007","DCM","Device"));
    CHECK_COND(tree.addContentItem(observerTypeNode, DSRTypes::AM_afterCurrent));


    DSRUIDRefTreeNode *observerUIDNode = new DSRUIDRefTreeNode(DSRTypes::RT_hasObsContext);
    CHECK_COND(observerUIDNode->setConceptName(DSRCodedEntryValue("121012","DCM","Device Observer UID")));
    CHECK_COND(observerUIDNode->setValue(deviceObserverUID));
    CHECK_COND(tree.addContentItem(observerUIDNode, DSRTypes::AM_afterCurrent));

    if(deviceObserverName){
      DSRTextTreeNode *observerNameNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
      CHECK_COND(observerNameNode->setConceptName(DSRCodedEntryValue("121013","DCM","Device Observer Name")));
      CHECK_COND(observerNameNode->setValue(deviceObserverName));
      CHECK_COND(tree.addContentItem(observerNameNode, DSRTypes::AM_afterCurrent));
    }

    if(deviceObserverManufacturer){
      DSRTextTreeNode *observerManufacturerNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
      CHECK_COND(observerManufacturerNode->setConceptName(DSRCodedEntryValue("121014","DCM","Device Observer Manufacturer")));
      CHECK_COND(observerManufacturerNode->setValue(deviceObserverManufacturer));
      CHECK_COND(tree.addContentItem(observerManufacturerNode, DSRTypes::AM_afterCurrent));
    }

    if(deviceObserverModelName){
      DSRTextTreeNode *observerModelNameNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
      CHECK_COND(observerModelNameNode->setConceptName(DSRCodedEntryValue("121015","DCM","Device Observer Model Name")));
      CHECK_COND(observerModelNameNode->setValue(deviceObserverModelName));
      CHECK_COND(tree.addContentItem(observerModelNameNode, DSRTypes::AM_afterCurrent));
    }

    if(deviceObserverSerialNumber){
      DSRTextTreeNode *observerSerialNumberNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
      CHECK_COND(observerSerialNumberNode->setConceptName(DSRCodedEntryValue("121016","DCM","Device Observer Serial Number")));
      CHECK_COND(observerSerialNumberNode->setValue(deviceObserverSerialNumber));
      CHECK_COND(tree.addContentItem(observerSerialNumberNode, DSRTypes::AM_afterCurrent));
    }
}

void AddPersonObserverContext(DSRDocumentTree &tree,
                        const char* personObserverName)
{
    DSRCodeTreeNode *observerTypeNode = new DSRCodeTreeNode(DSRTypes::RT_hasObsContext);
    CHECK_COND(observerTypeNode->setConceptName(DSRCodedEntryValue("121005","DCM","Observer Type")));
    CHECK_COND(observerTypeNode->setCode("121006","DCM","Person"));
    CHECK_COND(tree.addContentItem(observerTypeNode, DSRTypes::AM_afterCurrent));


    DSRPNameTreeNode *observerNameNode = new DSRPNameTreeNode(DSRTypes::RT_hasObsContext);
    CHECK_COND(observerNameNode->setConceptName(DSRCodedEntryValue("121008","DCM","Person Observer Name")));
    CHECK_COND(observerNameNode->setValue(personObserverName));
    CHECK_COND(tree.addContentItem(observerNameNode, DSRTypes::AM_afterCurrent));
}

void AddImageLibrary(DSRDocumentTree &tree, std::vector<DcmDataset*> &imageDatasets){
  DSRContainerTreeNode *libContainerNode = new DSRContainerTreeNode(DSRTypes::RT_contains);
  CHECK_COND(libContainerNode->setConceptName(DSRCodedEntryValue("111028", "DCM", "Image Library")));
  CHECK_COND(libContainerNode->setTemplateIdentification("1600","DCMR"));
  CHECK_COND(tree.addContentItem(libContainerNode));

  DSRContainerTreeNode *libGroupNode = new DSRContainerTreeNode(DSRTypes::RT_contains);
  CHECK_COND(libGroupNode->setConceptName(DSRCodedEntryValue("126200", "DCM", "Image Library Group")));
  CHECK_COND(tree.addContentItem(libGroupNode, DSRTypes::AM_belowCurrent));

  // factor out image library entry descriptors
  AddImageLibraryEntryDescriptors(tree, imageDatasets[0]);

  for(int i=0;i<imageDatasets.size();i++){
    AddImageLibraryEntry(tree, imageDatasets[i]);
  }
  tree.goUp();
  tree.goUp();
}

void AddImageLibraryDateDescriptor(DSRDocumentTree& dest, DcmDataset* src, const DcmTagKey tag,
    DSRTypes::E_AddMode& mode, const DSRCodedEntryValue code){

  DcmElement *element;
  OFString elementOFString;

  if(src->findAndGetElement(tag, element).good()){
      element->getOFString(elementOFString, 0);
      CHECK_NONZERO(dest.addContentItem(DSRTypes::RT_hasAcqContext,
                                                  DSRTypes::VT_Date,
                                                  mode));
      dest.getCurrentContentItem().setConceptName(code);
      dest.getCurrentContentItem().setStringValue(elementOFString.c_str());
  }
}

OFCondition AddImageLibraryDescriptorFromTag(DSRDocumentTree& dest, DcmDataset* src, const DcmTagKey tag,
    DSRTypes::E_RelationshipType rel, DSRTypes::E_ValueType type, DSRTypes::E_AddMode mode,
    const DSRCodedEntryValue code){
  DcmElement *element;
  OFString elementOFString;

  if(src->findAndGetElement(tag, element).good()){
      element->getOFString(elementOFString, 0);
      std::cout << "Tag as string: " << elementOFString << std::endl;
      CHECK_NONZERO(dest.addContentItem(rel,type,mode));
      dest.getCurrentContentItem().setConceptName(code);
      if(!dest.getCurrentContentItem().setStringValue(elementOFString.c_str()).good())
        std::cout << "Failed to set value " << elementOFString << " from tag key " << tag << std::endl;
      return EC_Normal;
  } else {
    std::cout << "Failed to find tag " << tag << std::endl;
    return EC_IllegalCall;
  }
}

void AddImageLibraryEntryDescriptors(DSRDocumentTree& tree, DcmDataset* dcm){

  DSRCodedEntryValue codedValue;
  DSRTypes::E_AddMode addMode = DSRTypes::AM_belowCurrent;
  DcmItem *sequenceItem;
  DcmElement *element;
  OFString elementOFString;

  // Image Laterality
  if(dcm->findAndGetSequenceItem(DCM_ImageLaterality,sequenceItem).good()){
     codedValue.readSequence(*dcm, DCM_ImageLaterality,"2");

     DSRCodeTreeNode *codeNode = new DSRCodeTreeNode(DSRTypes::RT_hasAcqContext);
     addMode = DSRTypes::AM_afterCurrent;
     CHECK_COND(codeNode->setConceptName(DSRCodedEntryValue("111027","DCM","Image Laterality")));
     CHECK_COND(codeNode->setCode(codedValue.getCodeValue(), codedValue.getCodingSchemeDesignator(),
                               codedValue.getCodeMeaning()));
     CHECK_COND(tree.addContentItem(codeNode, addMode));
     addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;
  }

  // Image View
  if(dcm->findAndGetSequenceItem(DCM_ViewCodeSequence,sequenceItem).good()){
    //findAndGetCodedValueFromSequenceItem(sequenceItem,codedValue);
    codedValue.readSequence(*dcm, DCM_ViewCodeSequence, "2");

    DSRCodeTreeNode *codeNode = new DSRCodeTreeNode(DSRTypes::RT_hasAcqContext);
    addMode = DSRTypes::AM_afterCurrent;
    CHECK_COND(codeNode->setConceptName(DSRCodedEntryValue("111031","DCM","Image View")));
    CHECK_COND(codeNode->setCode(codedValue.getCodeValue(), codedValue.getCodingSchemeDesignator(),
                              codedValue.getCodeMeaning()));
    CHECK_COND(tree.addContentItem(codeNode, addMode));

    addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

    if(codedValue.readSequence(*dcm, DCM_ViewModifierCodeSequence, "2").good()){

      DSRCodeTreeNode *codeNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
      addMode = DSRTypes::AM_belowCurrent;
      CHECK_COND(codeNode->setConceptName(DSRCodedEntryValue("111032","DCM","Image View Modifier")));
      CHECK_COND(codeNode->setCode(codedValue.getCodeValue(), codedValue.getCodingSchemeDesignator(),
                                codedValue.getCodeMeaning()));
      CHECK_COND(tree.addContentItem(codeNode, addMode));

      tree.goUp();
    }
  }


  // Patient Orientation - Row and Column separately
  if(dcm->findAndGetElement(DCM_PatientOrientation, element).good()){

      element->getOFString(elementOFString, 0);
      DSRTextTreeNode *textNode = new DSRTextTreeNode(DSRTypes::RT_hasAcqContext);
      CHECK_COND(textNode->setConceptName(DSRCodedEntryValue("111044","DCM","Patient Orientation Row")));
      CHECK_COND(textNode->setValue(elementOFString));
      CHECK_COND(tree.addContentItem(textNode, addMode));
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      tree.getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111044","DCM","Patient Orientation Row"));
      tree.getCurrentContentItem().setStringValue(elementOFString.c_str());

      element->getOFString(elementOFString, 1);
      textNode = new DSRTextTreeNode(DSRTypes::RT_hasAcqContext);
      CHECK_COND(textNode->setConceptName(DSRCodedEntryValue("111043","DCM","Patient Orientation Column")));
      CHECK_COND(textNode->setValue(elementOFString));
      CHECK_COND(tree.addContentItem(textNode, addMode));
  }

  // Modality
  // warning: this is fixed, since all measurements refer to PET, but should be
  // more generic for more general case
  DSRCodeTreeNode *modNode = new DSRCodeTreeNode(DSRTypes::RT_hasAcqContext);
  CHECK_COND(modNode->setConceptName(DSRCodedEntryValue("121139","DCM","Modality")));
  CHECK_COND(modNode->setCode("PT", "DCM","Positron emission tomography"));
  CHECK_COND(tree.addContentItem(modNode, DSRTypes::AM_belowCurrent));
  addMode = DSRTypes::AM_afterCurrent;

  // Study date
  if(AddImageLibraryDescriptorFromTag(tree, dcm, DCM_StudyDate,
      DSRTypes::RT_hasAcqContext, DSRTypes::VT_Date, addMode,
      DSRCodedEntryValue("111060","DCM","Study Date")).good()){
    addMode = DSRTypes::AM_afterCurrent;
  }

  // Study time
  if(AddImageLibraryDescriptorFromTag(tree, dcm, DCM_StudyTime,
      DSRTypes::RT_hasAcqContext, DSRTypes::VT_Time, addMode,
      DSRCodedEntryValue("111061","DCM","Study Time")).good()){
    addMode = DSRTypes::AM_afterCurrent;
  };

  // Content date
  if(AddImageLibraryDescriptorFromTag(tree, dcm, DCM_ContentDate,
      DSRTypes::RT_hasAcqContext, DSRTypes::VT_Date, addMode,
      DSRCodedEntryValue("111018","DCM","Content Date")).good()){
    addMode = DSRTypes::AM_afterCurrent;
  }

  // Content time
  if(AddImageLibraryDescriptorFromTag(tree, dcm, DCM_ContentTime,
      DSRTypes::RT_hasAcqContext, DSRTypes::VT_Time, addMode,
      DSRCodedEntryValue("111019","DCM","Content Time")).good()){
    addMode = DSRTypes::AM_afterCurrent;
  }

  if(AddImageLibraryDescriptorFromTag(tree, dcm, DCM_AcquisitionDate,
      DSRTypes::RT_hasAcqContext, DSRTypes::VT_Date, addMode,
      DSRCodedEntryValue("126201","DCM","Acquisition Date")).good()){
    addMode = DSRTypes::AM_afterCurrent;
  }

  if(AddImageLibraryDescriptorFromTag(tree, dcm, DCM_AcquisitionTime,
      DSRTypes::RT_hasAcqContext, DSRTypes::VT_Time, addMode,
      DSRCodedEntryValue("126202","DCM","Acquisition Time")).good()){
    addMode = DSRTypes::AM_afterCurrent;
  }

  if(AddImageLibraryDescriptorFromTag(tree, dcm, DCM_FrameOfReferenceUID,
      DSRTypes::RT_hasAcqContext, DSRTypes::VT_UIDRef, addMode,
      DSRCodedEntryValue("112227","DCM","Frame of Reference UID")).good()){
    addMode = DSRTypes::AM_afterCurrent;
  }

  if(dcm->findAndGetElement(DCM_Rows, element).good()){
      element->getOFString(elementOFString, 0);
      CHECK_NONZERO(tree.addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    addMode));
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      tree.getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110910","DCM","Pixel Data Rows"));
      tree.getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{pixels}","UCUM","pixels")));

      dcm->findAndGetElement(DCM_Columns, element);
      element->getOFString(elementOFString, 0);
      CHECK_NONZERO(tree.addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent));
      tree.getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110911","DCM","Pixel Data Columns"));
      tree.getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{pixels}","UCUM","pixels")));
  }

  // TODO: populate PET descriptors, but Reinhard says nothing from that is
  // needed and is duplication
  if(AddImageLibraryDescriptorFromTag(tree, dcm, DCM_RadiopharmaceuticalStartTime,
      DSRTypes::RT_hasAcqContext, DSRTypes::VT_DateTime, addMode,
      DSRCodedEntryValue("123003","DCM","Radiopharmaceutical Start Date Time")).good()){
    addMode = DSRTypes::AM_afterCurrent;
  }

  if(dcm->findAndGetElement(DCM_RadionuclideTotalDose, element).good()){
      element->getOFString(elementOFString, 0);
      CHECK_NONZERO(tree.addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    addMode));
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      tree.getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("123006","DCM","Radionuclide Total Dose"));
      tree.getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("Bq","UCUM","Bq")));
  }

  // Warning: hard-coded, specific to Iowa use case, not available in the
  // source PET dataset!
  {
      CHECK_NONZERO(tree.addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Code,
                                    addMode));
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      tree.getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("C-10072","SRT","Radionuclide"));
      tree.getCurrentContentItem().setCodeValue(DSRCodedEntryValue("C-111A1","SRT","^18^Fluorine"));
  }

  {
      CHECK_NONZERO(tree.addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Code,
                                    addMode));
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      tree.getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("F-61FDB","SRT","Radiopharmaceutical agent"));
      tree.getCurrentContentItem().setCodeValue(DSRCodedEntryValue("C-B1031","SRT","Fluorodeoxyglucose F^18^"));
  }


  return;
  if(addMode==DSRTypes::AM_afterCurrent)
    tree.goUp();

}

void AddImageLibraryEntry(DSRDocumentTree &tree, DcmDataset *dcm){
  {
    DSRImageTreeNode *imageNode = new DSRImageTreeNode(DSRTypes::RT_contains);
    OFString classUIDStr, instanceUIDStr;
    DcmElement *classUIDElt, *instanceUIDElt;
    CHECK_COND(dcm->findAndGetElement(DCM_SOPClassUID, classUIDElt));
    CHECK_COND(dcm->findAndGetElement(DCM_SOPInstanceUID, instanceUIDElt));
    classUIDElt->getOFString(classUIDStr, 0);
    instanceUIDElt->getOFString(instanceUIDStr, 0);
    CHECK_COND(imageNode->setReference(classUIDStr, instanceUIDStr));
    CHECK_COND(tree.addContentItem(imageNode, DSRTypes::AM_afterCurrent));
  }
}

/* Warning: assume here that there is a line with measurements for each
 * segment, in the same order as segments appear in SEG
 */
void ReadMeasurements(std::string filename, std::vector<Measurements> &segmentMeasurements, QuantitiesDictionaryType &dict){
  std::cout << "Reading measurements from " << filename << std::endl;
  std::ifstream f;
  char fLine[10000];
  unsigned numSegments = 0, numMeasurementsPerSegment = 0;

  f.open(filename.c_str());
  std::vector<std::string> measurementCodes, measurementValues;
  f.getline(fLine, 10000);
  TokenizeString(fLine, measurementCodes, ",");

  while(!f.eof()){
    f.getline(fLine, 10000);
    measurementValues.clear();
    TokenizeString(fLine, measurementValues, ",");
    if(measurementValues.size() != measurementCodes.size()){
      OFLOG_DEBUG(logger, "Mismatch in parsing measurement files");
      return;
    }
    for(unsigned mPos=0;mPos<measurementCodes.size();mPos++){
      ROIMeasurementType m;
      std::string mCode = measurementCodes[mPos], mValue = measurementValues[mPos];
      if(dict.find(mCode) == dict.end()){
        std::cerr << "Failed to find mapping for " << mCode << std::endl;
        continue;
      }
      if(mValue == "nan"){
        std::cerr << "Skipping nan in the measurements list" << std::endl;
        continue;
      }
      m.QuantityCode = dict[mCode].QuantityCode;
      m.UnitsCode = dict[mCode].UnitsCode;
      m.Derivation = dict[mCode].Derivation;
      m.MeasurementValue = mValue;
      segmentMeasurements[numSegments].push_back(m);
      numMeasurementsPerSegment++;
    }
    numSegments++;
  }
  std::cout << "Read measurements for " << numSegments << " segments." << std::endl;
  std::cout << numMeasurementsPerSegment/numSegments << " measurements per segment" << std::endl;
}

void ReadMeasurementsForStructure(std::string filename, Measurements &measurements, QuantitiesDictionaryType &dict){

  std::cout << "Reading measurements from " << filename << std::endl;
  std::ifstream f;

  f.open(filename.c_str());
  while(!f.eof()){
    char fLine[256];
    std::vector<std::string> tokens;
    f.getline(fLine, 256);
    if(!f.eof()){
      ROIMeasurementType m;
      //f.getline(fLine, 256);
      //if(f.eof())
      //  break;
      //std::cout << "  " << std::string(fLine) << std::endl;
      TokenizeString(std::string(fLine), tokens, " ");
      if(dict.find(tokens[0]) == dict.end()){
        OFLOG_DEBUG(logger, "Failed to find mapping for " << tokens[0]);
        continue;
      }
      m.QuantityCode = dict[tokens[0]].QuantityCode;
      m.UnitsCode = dict[tokens[0]].UnitsCode;
      m.MeasurementValue = tokens[2].c_str();
      measurements.push_back(m);
    }
  }
}

/* Map measurement code used internally by QuantitativeIndexCalculator to
 * DCM codes for quantities and units
 */
void MapMeasurementsCodeToQuantityAndUnits(std::string &code, DSRCodedEntryValue &quantity, DSRCodedEntryValue &units){
  throw -3;
}

void MapFileNameToStructureCode(std::string filename, DSRCodedEntryValue& code){
  if(filename.find("liver") != std::string::npos){
    code = DSRCodedEntryValue("T-62000","SRT","Liver");
  } else if(filename.find("cerebellum") != std::string::npos){
    code = DSRCodedEntryValue("T-A6000","SRT","Cerebellum");
  } else if(filename.find("aorta") != std::string::npos){
    code = DSRCodedEntryValue("T-42300","SRT","Aortic arch");
  } else if(filename.find("tumor") != std::string::npos){
    code = DSRCodedEntryValue("M-8FFFF","SRT","Neoplasm");
  } else if(filename.find("lymph") != std::string::npos){
    code = DSRCodedEntryValue("T-C4000","SRT","Lymph node");
  } else {
    OFLOG_FATAL(logger, "Failed to map region file name to anatomic structure!");
    throw -2;
  }
}

void PopulateMeasurementsGroup(DSRDocumentTree &tree, DSRContainerTreeNode *groupNode, DSRCodedEntryValue &anatomicalStructureCode, Measurements &measurements,
                               std::vector<DcmDataset*> petDatasets,
                               DcmDataset* rwvmDataset,
                               DcmDataset* segDataset,
                               const char* timePointId,
                               const char* sessionId,
                               unsigned segmentNumber,
                               const char* trackingID,
                               const char* trackingUID,
                               DSRCodedEntryValue &finding,
                               DSRCodedEntryValue &findingSite){
  std::cout << "Populating group ..." << std::endl;

  // FYI: session stuff is non-standard ...
  DSRTextTreeNode *sessionNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
  CHECK_COND(sessionNode->setConceptName(DSRCodedEntryValue("C67447","NCIt","Activity Session")));
  CHECK_COND(sessionNode->setValue(sessionId));
  CHECK_COND(tree.addContentItem(sessionNode, DSRTypes::AM_belowCurrent));

  DSRTextTreeNode *trackingIDNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
  CHECK_COND(trackingIDNode->setConceptName(DSRCodedEntryValue("112039","DCM","Tracking Identifier")));
  CHECK_COND(trackingIDNode->setValue(trackingID));
  CHECK_COND(tree.addContentItem(trackingIDNode,
                                  DSRTypes::AM_afterCurrent));

  DSRUIDRefTreeNode *trackingUIDNode = new DSRUIDRefTreeNode(DSRTypes::RT_hasObsContext);
  std::cout << "Setting tracking UID: " << trackingUID << std::endl;
  CHECK_COND(trackingUIDNode->setValue(trackingUID));
  CHECK_COND(trackingUIDNode->setConceptName(DSRCodedEntryValue("112040","DCM","Tracking Unique Identifier")));
  CHECK_COND(tree.addContentItem(trackingUIDNode, DSRTypes::AM_afterCurrent));

  // Finding and finding site should be populated only if the code is initialized! Assuming
  // code 0 is not initialized (line 288)
  if(std::string(finding.getCodeValue().c_str()) != std::string("0")){
    DSRCodeTreeNode *findingNode = new DSRCodeTreeNode(DSRTypes::RT_contains);
    CHECK_COND(findingNode->setConceptName(DSRCodedEntryValue("121071","DCM","Finding")));
    CHECK_COND(findingNode->setCode(finding.getCodeValue(), finding.getCodingSchemeDesignator(), finding.getCodeMeaning()));
    CHECK_COND(tree.addContentItem(findingNode, DSRTypes::AM_afterCurrent));
  }

  DSRTextTreeNode *timepointNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
  CHECK_COND(timepointNode->setConceptName(DSRCodedEntryValue("C2348792","UMLS","Time Point")));
  CHECK_COND(timepointNode->setValue(timePointId));
  CHECK_COND(tree.addContentItem(timepointNode, DSRTypes::AM_afterCurrent));

  /* debugging
  if(0){
    DcmElement *e;
    char* segInstanceUIDPtr;
    CHECK_COND(segDataset->findAndGetElement(DCM_SOPInstanceUID, e));
    e->getString(segInstanceUIDPtr);
    std::cout << "SEG UID: " << segInstanceUIDPtr << std::endl;

    DSRImageReferenceValue refValue(UID_SegmentationStorage, segInstanceUIDPtr);
    //refValue.getSegmentList().putString("1"); // TODO: this will need to be fixed - refer to the actual segments in the seg object
    refValue.getSegmentList().addItem(1); // TODO: this will need to be fixed - refer to the actual segments in the seg object
    CHECK_COND(tree.addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Image));
    tree.getCurrentContentItem().setConceptName(DSRCodedEntryValue("121191","DCM","Referenced Segment"));
    tree.getCurrentContentItem().setImageReference(refValue);
  } */

  if(1){
    DcmElement *e;
    char* segInstanceUIDPtr;
    CHECK_COND(segDataset->findAndGetElement(DCM_SOPInstanceUID, e));
    e->getString(segInstanceUIDPtr);
    std::cout << "SEG UID: " << segInstanceUIDPtr << std::endl;
    DSRImageTreeNode *segNode = new DSRImageTreeNode(DSRTypes::RT_contains);
    CHECK_COND(segNode->setConceptName(DSRCodedEntryValue("121191","DCM","Referenced Segment")));
    DSRImageReferenceValue refValue(UID_SegmentationStorage, segInstanceUIDPtr);
    refValue.getSegmentList().addItem(segmentNumber);
    CHECK_COND(segNode->setValue(refValue));
    CHECK_COND(tree.addContentItem(segNode, DSRTypes::AM_afterCurrent));
  }


  {
    DSRUIDRefTreeNode *seriesUIDNode = new DSRUIDRefTreeNode(DSRTypes::RT_contains);
    //CHECK_COND(trackingUIDNode);
    char* seriesInstanceUID;
    DcmElement *e;
    CHECK_COND(petDatasets[0]->findAndGetElement(DCM_SeriesInstanceUID, e));
    e->getString(seriesInstanceUID);
    CHECK_COND(seriesUIDNode->setValue(seriesInstanceUID));
    CHECK_COND(seriesUIDNode->setConceptName(DSRCodedEntryValue("121232","DCM","Source series for image segmentation")));
    CHECK_COND(tree.addContentItem(seriesUIDNode, DSRTypes::AM_afterCurrent));
  }

  {
    DcmElement *e;
    char* rwvmInstanceUIDPtr;
    CHECK_COND(rwvmDataset->findAndGetElement(DCM_SOPInstanceUID, e));
    e->getString(rwvmInstanceUIDPtr);
    DSRCompositeTreeNode *rwvmNode = new DSRCompositeTreeNode(DSRTypes::RT_contains);
    DSRCompositeReferenceValue refValue(UID_RealWorldValueMappingStorage, rwvmInstanceUIDPtr);
    rwvmNode->setValue(refValue);
    CHECK_COND(rwvmNode->setConceptName(DSRCodedEntryValue("126100","DCM","Real World Value Map used for measurement")));
    CHECK_COND(tree.addContentItem(rwvmNode, DSRTypes::AM_afterCurrent));
  }

  DSRCodeTreeNode *modNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
  CHECK_COND(modNode->setConceptName(DSRCodedEntryValue("G-C036","SRT","Measurement Method")));
  CHECK_COND(modNode->setCode("126410", "DCM","SUV body weight calculation method"));
  CHECK_COND(tree.addContentItem(modNode, DSRTypes::AM_afterCurrent));

  if(std::string(findingSite.getCodeValue().c_str()) != std::string("0")){
    // Special handling here since Aortic arch is segmented in CT, but measurements are done in resampled PET
    // This was later decided to not include, since only one Measurement Method
    // concept modifier is allowed
    /*
    if(findingSite.getCodeMeaning() == "Aortic arch"){
      DSRCodeTreeNode *findingSiteNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
      CHECK_COND(findingSiteNode->setConceptName(DSRCodedEntryValue("G-C036","SRT","Measurement Method")));
      CHECK_COND(findingSiteNode->setCode("250157","99PMP","Source images resampled to resolution of segmentation"));
      CHECK_COND(tree.addContentItem(findingSiteNode, DSRTypes::AM_afterCurrent));
    }
    */
    DSRCodeTreeNode *findingSiteNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
    CHECK_COND(findingSiteNode->setConceptName(DSRCodedEntryValue("G-C0E3","SRT","Finding Site")));
    CHECK_COND(findingSiteNode->setCode(findingSite.getCodeValue(), findingSite.getCodingSchemeDesignator(), findingSite.getCodeMeaning()));
    CHECK_COND(tree.addContentItem(findingSiteNode, DSRTypes::AM_afterCurrent));
  }

  std::cout << "Group has " << measurements.size() << " measurements" << std::endl;

  for(int i=0;i<measurements.size();i++){
    DSRNumTreeNode *measurementNode = new DSRNumTreeNode(DSRTypes::RT_contains);

    if(std::string(measurements[i].UnitsCode.getCodeValue().c_str()) == "{SUVbw}g/ml"
        && measurements[i].Derivation){
      CHECK_COND(measurementNode->setConceptName(DSRCodedEntryValue("126401","DCM","SUVbw")));
      DSRNumericMeasurementValue measurementValue(measurements[i].MeasurementValue.c_str(),
                                                  measurements[i].UnitsCode);
      CHECK_COND(measurementNode->setValue(measurementValue));
      CHECK_COND(tree.addContentItem(measurementNode, DSRTypes::AM_afterCurrent));
      DSRCodeTreeNode *modNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
      CHECK_COND(modNode->setConceptName(DSRCodedEntryValue("121401","DCM","Derivation")));
      DSRCodedEntryValue quantityCode = measurements[i].QuantityCode;
      CHECK_COND(modNode->setCode(quantityCode.getCodeValue(), quantityCode.getCodingSchemeDesignator(), quantityCode.getCodeMeaning()));
      CHECK_COND(tree.addContentItem(modNode, DSRTypes::AM_belowCurrent));
      tree.goUp();
    } else {
      DSRCodedEntryValue quantityCode = measurements[i].QuantityCode;
      CHECK_COND(measurementNode->setConceptName(quantityCode));
      //Code(quantityCode.getCodeValue(), quantityCode.getCodingSchemeDesignator(), quantityCode.getCodeMeaning()));
      DSRNumericMeasurementValue measurementValue(measurements[i].MeasurementValue.c_str(),
                                                  measurements[i].UnitsCode);
      measurementNode->setValue(measurementValue);
      CHECK_COND(tree.addContentItem(measurementNode, DSRTypes::AM_afterCurrent));

      if(measurements[i].QuantityCode.getCodeMeaning() == "Volume"){
        DSRCodeTreeNode *modNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
        CHECK_COND(modNode->setConceptName(DSRCodedEntryValue("G-C036","SRT","Measurement Method")));
        CHECK_COND(modNode->setCode("126030", "DCM", "Sum of segmented voxel volumes"));
        CHECK_COND(tree.addContentItem(modNode, DSRTypes::AM_belowCurrent));
        tree.goUp();
      }
      // TODO: add special case for Volume modifier
    }
  }
}

void ReadQuantitiesDictionary(std::string filename, QuantitiesDictionaryType &dict){
  std::ifstream f;
  f.open(filename.c_str());
  //f.open(filename);
  char fLine[256];
  f.getline(fLine,256); // skip header
  while(!f.eof()){
    QuantityUnitsPairType entry;
    f.getline(fLine, 256);
    if(f.eof())
      break;
    std::vector<std::string> tokens;
    TokenizeString(fLine, tokens, ",");
    entry.QuantityCode = DSRCodedEntryValue(tokens[1].c_str(),tokens[2].c_str(),tokens[3].c_str());
    entry.UnitsCode = DSRCodedEntryValue(tokens[4].c_str(),tokens[5].c_str(),tokens[6].c_str());
    entry.Derivation = bool(atoi(tokens[7].c_str()));
    dict[tokens[0]] = entry;
  }
}

void AddCodingScheme(DSRDocument *doc,
                     const char* id,
                     const char* uidRoot,
                     const char* name,
                     const char* org){
  doc->getCodingSchemeIdentification().addItem(id);
  doc->getCodingSchemeIdentification().setCodingSchemeUID(uidRoot);
  doc->getCodingSchemeIdentification().setCodingSchemeName(name);
  if(org)
    doc->getCodingSchemeIdentification().setCodingSchemeResponsibleOrganization(org);
}
