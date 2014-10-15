///// STL includes
#include <iostream>
#include <string>
#include <vector>

#include "../Common/QIICRUIDs.h"

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

#include <stdlib.h>

#include "../Iowa2DICOMVersionConfigure.h"

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


#define WARN_IF_ERROR(FunctionCall,Message) if(!FunctionCall) std::cout << "Return value is 0 for " << Message << std::endl;

int getReferencedInstances(DcmDataset* dataset,
                            std::vector<std::string> &classUIDs,
                            std::vector<std::string> &instanceUIDs);

struct QuantityUnitsPairType {
  DSRCodedEntryValue QuantityCode;
  DSRCodedEntryValue UnitsCode;
};

typedef std::map<std::string, QuantityUnitsPairType> QuantitiesDictionaryType;

struct ROIMeasurementType {
  DSRCodedEntryValue QuantityCode;
  DSRCodedEntryValue UnitsCode;
  std::string MeasurementValue;
};

// coded pairs of quantity/units
typedef std::vector<ROIMeasurementType> Measurements;
typedef std::map<DSRCodedEntryValue, Measurements> StructureToMeasurementsType;


void InitializeRootNode(DSRDocumentTree&);

void AddLanguageOfContent(DSRDocumentTree&);
//void AddObservationContext(DSRDocumentTree&);
void AddObserverContext(DSRDocumentTree&,
                        const char* deviceObserverUID,
                        const char* deviceObserverName,
                        const char* deviceObserverManufacturer,
                        const char* deviceObserverModelName,
                        const char* deviceObserverSerialNumber);
void AddImageLibrary(DSRDocumentTree&, std::vector<DcmDataset*>&);
void AddImageLibraryEntry(DSRDocumentTree&, DcmDataset*);

void PopulateMeasurementsGroup(DSRDocumentTree&, DSRContainerTreeNode*, DSRCodedEntryValue&, Measurements&,
                               std::vector<DcmDataset*> petDatasets,
                               DcmDataset* rwvmDataset,
                               DcmDataset* segDataset);

void ReadMeasurementsForStructure(std::string filename, Measurements &measurementsList, QuantitiesDictionaryType&);
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
  doc->setSeriesDescription("ROI quantitative measurement");

  DSRDocumentTree &tree = doc->getTree();

  InitializeRootNode(tree);

  AddLanguageOfContent(tree);

  // TODO: initialize to more meaningful values
  AddObserverContext(tree, QIICR_DEVICE_OBSERVER_UID,
                     Iowa2DICOM_WC_URL,
                     "QIICR", Iowa2DICOM_WC_REVISION, "0");

  {
    DSRCodeTreeNode *procedureCode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
    CHECK_COND(procedureCode->setConceptName(DSRCodedEntryValue("121058","DCM","Procedure reported")));
    CHECK_COND(procedureCode->setCode("P5-080FF","SRT","PET/CT FDG imaging of the whole body"));
    CHECK_EQUAL(tree.addContentItem(procedureCode, DSRTypes::AM_afterCurrent), procedureCode);
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
    CHECK_COND(measurementsContainer->setConceptName(DSRCodedEntryValue("250200","99PMP","Measurements")));
    CHECK_EQUAL(tree.addContentItem(measurementsContainer, DSRTypes::AM_afterCurrent), measurementsContainer);

    // create a volumetric ROI measurements container (TID 1411) for each ROI, with each measurement container
    //  keeping all of the measurements for that ROI
    for(unsigned i=0;i<measurementsFileNames.size();i++){
      // for now, assign the codes manually, but when SEGs are ready, get them from DICOM
      DSRCodedEntryValue anatomicalStructureCode;
      Measurements measurementsList;

      MapFileNameToStructureCode(measurementsFileNames[i], anatomicalStructureCode);
      ReadMeasurementsForStructure(measurementsFileNames[i], measurementsList, quantitiesDictionary);

      DSRContainerTreeNode *measurementsGroup = new DSRContainerTreeNode(DSRTypes::RT_contains);
      CHECK_COND(measurementsGroup->setConceptName(DSRCodedEntryValue("125007","DCM","Measurement Group")));
      CHECK_EQUAL(tree.addContentItem(measurementsGroup, DSRTypes::AM_belowCurrent), measurementsGroup);

      PopulateMeasurementsGroup(tree, measurementsGroup, anatomicalStructureCode, measurementsList,
                                petDatasets, rwvmDataset, segDataset);

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

  AddCodingScheme(doc, "99QIICR", QIICR_CODING_SCHEME_UID_ROOT, "QIICR Coding Scheme", "Quantitative Imaging for Cancer Research, http://qiicr.org");
  AddCodingScheme(doc, "99PMP", "1.3.6.1.4.1.5962.98.1", "PixelMed Publishing");

  std::cout << "Before writing the dataset" << std::endl;
  doc->write(*datasetSR);

/*
  DcmItem i1, i2;
  DcmModuleHelpers::copySOPCommonModule(i1,i2);

  DcmModuleHelpers::copyPatientModule(*petDatasets[0],*datasetSR);
  DcmModuleHelpers::copyPatientStudyModule(*petDatasets[0],*datasetSR);
  DcmModuleHelpers::copyGeneralStudyModule(*petDatasets[0],*datasetSR);
*/
  if(outputSRFileName.size()){
    std::cout << "Will save the result to " << outputSRFileName << std::endl;
    fileformatSR->saveFile(outputSRFileName.c_str(), EXS_LittleEndianExplicit);
  }

  return 0;
}

void InitializeRootNode(DSRDocumentTree &tree){

  DSRContainerTreeNode *rootNode = new DSRContainerTreeNode(DSRTypes::RT_isRoot);
  CHECK_EQUAL(tree.addContentItem(rootNode), rootNode);

  CHECK_COND(rootNode->setTemplateIdentification("1000", "99QIICR"));
  rootNode->setConceptName(DSRCodedEntryValue("10001","99QIICR","Quantitative measurement report"));
}


void AddLanguageOfContent(DSRDocumentTree &tree){

  DSRCodeTreeNode *langNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);

  langNode->setConceptName(DSRCodedEntryValue("121049", "DCM", "Language of Content Item and Descendants"));
  langNode->setCode("eng", "RFC3066", "English");

  CHECK_EQUAL(tree.addContentItem(langNode, DSRTypes::AM_belowCurrent), langNode);

  DSRCodeTreeNode *countryNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
  CHECK_COND(countryNode->setConceptName(DSRCodedEntryValue("121046", "DCM", "Country of Language")));
  CHECK_COND(countryNode->setCode("US","ISO3166_1","United States"));

  CHECK_EQUAL(tree.addContentItem(countryNode, DSRTypes::AM_belowCurrent), countryNode);

  tree.goUp();
}

void AddObserverContext(DSRDocumentTree &tree,
                        const char* deviceObserverUID,
                        const char* deviceObserverName,
                        const char* deviceObserverManufacturer,
                        const char* deviceObserverModelName,
                        const char* deviceObserverSerialNumber){

    DSRCodeTreeNode *observerTypeNode = new DSRCodeTreeNode(DSRTypes::RT_hasObsContext);
    CHECK_COND(observerTypeNode->setConceptName(DSRCodedEntryValue("121005","DCM","Observer Type")));
    CHECK_COND(observerTypeNode->setCode("121007","DCM","Device"));
    CHECK_EQUAL(tree.addContentItem(observerTypeNode, DSRTypes::AM_afterCurrent), observerTypeNode);


    DSRUIDRefTreeNode *observerUIDNode = new DSRUIDRefTreeNode(DSRTypes::RT_hasObsContext);
    CHECK_COND(observerUIDNode->setConceptName(DSRCodedEntryValue("121012","DCM","Device Observer UID")));
    CHECK_COND(observerUIDNode->setValue(deviceObserverUID));
    CHECK_EQUAL(tree.addContentItem(observerUIDNode, DSRTypes::AM_afterCurrent), observerUIDNode);

    DSRTextTreeNode *observerNameNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
    CHECK_COND(observerNameNode->setConceptName(DSRCodedEntryValue("121013","DCM","Device Observer Name")));
    CHECK_COND(observerNameNode->setValue(deviceObserverName));
    CHECK_EQUAL(tree.addContentItem(observerNameNode, DSRTypes::AM_afterCurrent), observerNameNode);

    DSRTextTreeNode *observerManufacturerNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
    CHECK_COND(observerManufacturerNode->setConceptName(DSRCodedEntryValue("121014","DCM","Device Observer Manufacturer")));
    CHECK_COND(observerManufacturerNode->setValue(deviceObserverManufacturer));
    CHECK_EQUAL(tree.addContentItem(observerManufacturerNode, DSRTypes::AM_afterCurrent), observerManufacturerNode);

    DSRTextTreeNode *observerModelNameNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
    CHECK_COND(observerModelNameNode->setConceptName(DSRCodedEntryValue("121015","DCM","Device Observer Model Name")));
    CHECK_COND(observerModelNameNode->setValue(deviceObserverModelName));
    CHECK_EQUAL(tree.addContentItem(observerModelNameNode, DSRTypes::AM_afterCurrent), observerModelNameNode);

    DSRTextTreeNode *observerSerialNumberNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
    CHECK_COND(observerSerialNumberNode->setConceptName(DSRCodedEntryValue("121016","DCM","Device Observer Serial Number")));
    CHECK_COND(observerSerialNumberNode->setValue(deviceObserverSerialNumber));
    CHECK_EQUAL(tree.addContentItem(observerSerialNumberNode, DSRTypes::AM_afterCurrent), observerSerialNumberNode);
}

void AddImageLibrary(DSRDocumentTree &tree, std::vector<DcmDataset*> &imageDatasets){
  DSRContainerTreeNode *libContainerNode = new DSRContainerTreeNode(DSRTypes::RT_contains);
  CHECK_COND(libContainerNode->setConceptName(DSRCodedEntryValue("111028", "DCM", "Image Library")));
  CHECK_EQUAL(tree.addContentItem(libContainerNode), libContainerNode);

  for(int i=0;i<imageDatasets.size();i++){
    AddImageLibraryEntry(tree, imageDatasets[i]);
    tree.gotoNode(libContainerNode->getNodeID());
  }
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
    CHECK_EQUAL(tree.addContentItem(imageNode, DSRTypes::AM_belowCurrent), imageNode);
  }

  DSRCodedEntryValue codedValue;
  DSRTypes::E_AddMode addMode;
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
     CHECK_EQUAL(tree.addContentItem(codeNode, addMode), codeNode);
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
    CHECK_EQUAL(tree.addContentItem(codeNode, addMode), codeNode);

    addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

    if(codedValue.readSequence(*dcm, DCM_ViewModifierCodeSequence, "2").good()){

      DSRCodeTreeNode *codeNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
      addMode = DSRTypes::AM_belowCurrent;
      CHECK_COND(codeNode->setConceptName(DSRCodedEntryValue("111032","DCM","Image View Modifier")));
      CHECK_COND(codeNode->setCode(codedValue.getCodeValue(), codedValue.getCodingSchemeDesignator(),
                                codedValue.getCodeMeaning()));
      CHECK_EQUAL(tree.addContentItem(codeNode, addMode), codeNode);

      tree.goUp();
    }
  }


  // Patient Orientation - Row and Column separately
  if(dcm->findAndGetElement(DCM_PatientOrientation, element).good()){

      element->getOFString(elementOFString, 0);      
      DSRTextTreeNode *textNode = new DSRTextTreeNode(DSRTypes::RT_hasAcqContext);
      CHECK_COND(textNode->setConceptName(DSRCodedEntryValue("111044","DCM","Patient Orientation Row")));
      CHECK_COND(textNode->setValue(elementOFString));
      CHECK_EQUAL(tree.addContentItem(textNode, addMode), textNode);

      tree.getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111044","DCM","Patient Orientation Row"));
      tree.getCurrentContentItem().setStringValue(elementOFString.c_str());

      element->getOFString(elementOFString, 1);
      textNode = new DSRTextTreeNode(DSRTypes::RT_hasAcqContext);
      CHECK_COND(textNode->setConceptName(DSRCodedEntryValue("111043","DCM","Patient Orientation Column")));
      CHECK_COND(textNode->setValue(elementOFString));
      CHECK_EQUAL(tree.addContentItem(textNode, addMode), textNode);
  }

#if 0
  // Study date
  if(imgDataset->findAndGetElement(DCM_StudyDate, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                                  DSRTypes::VT_Date,
                                                  addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111060","DCM","Study Date"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString.c_str());
  }

  // Study time
  if(imgDataset->findAndGetElement(DCM_StudyTime, element).good()){

      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Time,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111061","DCM","Study Time"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString.c_str());
  }

  // Content date
  if(imgDataset->findAndGetElement(DCM_ContentDate, element).good()){

      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Date,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111018","DCM","Content Date"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString.c_str());
  }

  // Content time
  if(imgDataset->findAndGetElement(DCM_ContentTime, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Time,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111019","DCM","Content Time"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString.c_str());
  }

  // Pixel Spacing - horizontal and vertical separately
  if(imgDataset->findAndGetElement(DCM_PixelSpacing, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111026","DCM","Horizontal Pixel Spacing"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));

      element->getOFString(elementOFString, 1);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("111066","DCM","Vertical Pixel Spacing"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));
  }

  // Positioner Primary Angle
  if(imgDataset->findAndGetElement(DCM_PositionerPrimaryAngle, element).good()){

      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("112011","DCM","Positioner Primary Angle"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("deg","UCUM","degrees of plane angle")));

  }

  // Positioner Secondary Angle
  if(imgDataset->findAndGetElement(DCM_PositionerSecondaryAngle, element).good()){

      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("112012","DCM","Positioner Secondary Angle"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("deg","UCUM","degrees of plane angle")));
  }

  // TODO
  // Spacing between slices: May be computed from the Image Position (Patient) (0020,0032)
  // projected onto the normal to the Image Orientation (Patient) (0020,0037) if present;
  // may or may not be the same as the Spacing Between Slices (0018,0088) if present.

  // Slice thickness/
  if(imgDataset->findAndGetElement(DCM_SliceThickness, element).good()){

      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("112225","DCM","Slice Thickness"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));
  }

  // Frame of reference
  if(imgDataset->findAndGetElement(DCM_FrameOfReferenceUID, element).good()){

      element->getOFString(elementOFString,0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_UIDRef,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("112227","DCM","Frame of Reference UID"));
      doc->getTree().getCurrentContentItem().setStringValue(elementOFString);
  }

  // Image Position Patient
  if(imgDataset->findAndGetElement(DCM_ImagePositionPatient, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110901","DCM","Image Position (Patient) X"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));

      element->getOFString(elementOFString, 1);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110902","DCM","Image Position (Patient) Y"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));

      element->getOFString(elementOFString, 2);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110903","DCM","Image Position (Patient) Z"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("mm","UCUM","millimeter")));
  }

  // Image Orientation Patient
  if(imgDataset->findAndGetElement(DCM_ImageOrientationPatient, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110904","DCM","Image Orientation (Patient) Row X"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

      element->getOFString(elementOFString, 1);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110905","DCM","Image Orientation (Patient) Row Y"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

      element->getOFString(elementOFString, 2);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110906","DCM","Image Orientation (Patient) Row Z"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

      element->getOFString(elementOFString, 3);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110907","DCM","Image Orientation (Patient) Column X"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

      element->getOFString(elementOFString, 4);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110908","DCM","Image Orientation (Patient) Column Y"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

      element->getOFString(elementOFString, 5);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110909","DCM","Image Orientation (Patient) Column Z"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{-1:1}","UCUM","{-1:1}")));

  }

  // Image Orientation Patient
  if(imgDataset->findAndGetElement(DCM_Rows, element).good()){
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    addMode);
      addMode = addMode == DSRTypes::AM_belowCurrent ? DSRTypes::AM_afterCurrent : addMode;

      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110910","DCM","Pixel Data Rows"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{pixels}","UCUM","pixels")));

      imgDataset->findAndGetElement(DCM_Columns, element);
      element->getOFString(elementOFString, 0);
      doc->getTree().addContentItem(DSRTypes::RT_hasAcqContext,
                                    DSRTypes::VT_Num,
                                    DSRTypes::AM_afterCurrent);
      doc->getTree().getCurrentContentItem().setConceptName(
                  DSRCodedEntryValue("110911","DCM","Pixel Data Columns"));
      doc->getTree().getCurrentContentItem().setNumericValue(
                  DSRNumericMeasurementValue(elementOFString.c_str(),
                                             DSRCodedEntryValue("{pixels}","UCUM","pixels")));
  }


  doc->getTree().goUp(); // up to image level
  doc->getTree().goUp(); // up to image library container level
#endif
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
        OFLOG_FATAL(logger, "Failed to find mapping for " << tokens[0]);
        throw -4;
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

/*
 * Tokenize input string with space as a delimiter
 */
void TokenizeString(std::string str, std::vector<std::string> &tokens, std::string delimiter){
  // http://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
  size_t pos = 0;
  while((pos = str.find(delimiter)) != std::string::npos) {
    std::string token = str.substr(0,pos);
    tokens.push_back(token);
    str.erase(0, pos+delimiter.length());
  }
  tokens.push_back(str);
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
                               DcmDataset* segDataset){
  DSRTextTreeNode *trackingIDNode = new DSRTextTreeNode(DSRTypes::RT_hasObsContext);
  CHECK_COND(trackingIDNode->setConceptName(DSRCodedEntryValue("112039","DCM","Tracking Identifier")));
  CHECK_COND(trackingIDNode->setValue(anatomicalStructureCode.getCodeMeaning()));
  CHECK_EQUAL(tree.addContentItem(trackingIDNode,
                                  DSRTypes::AM_belowCurrent),trackingIDNode);

  DSRUIDRefTreeNode *trackingUIDNode = new DSRUIDRefTreeNode(DSRTypes::RT_hasObsContext);
  //CHECK_COND(trackingUIDNode);
  char trackingUID[128];
  dcmGenerateUniqueIdentifier(trackingUID, SITE_INSTANCE_UID_ROOT);
  CHECK_COND(trackingUIDNode->setValue(trackingUID));
  CHECK_COND(trackingUIDNode->setConceptName(DSRCodedEntryValue("112040","DCM","Tracking Unique Identifier")));
  CHECK_EQUAL(tree.addContentItem(trackingUIDNode, DSRTypes::AM_afterCurrent),
              trackingUIDNode);

  {
    DcmElement *e;
    char* segInstanceUIDPtr;
    CHECK_COND(segDataset->findAndGetElement(DCM_SOPInstanceUID, e));
    e->getString(segInstanceUIDPtr);
    DSRImageTreeNode *segNode = new DSRImageTreeNode(DSRTypes::RT_contains);
    segNode->setConceptName(DSRCodedEntryValue("121191","DCM","Referenced Segment"));
    DSRImageReferenceValue refValue(UID_SegmentationStorage, segInstanceUIDPtr);
    refValue.getSegmentList().addItem(99); // TODO: this will need to be fixed - refer to the actual segments in the seg object
    segNode->setValue(refValue);
    CHECK_EQUAL(tree.addContentItem(segNode, DSRTypes::AM_afterCurrent), segNode);
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
    CHECK_EQUAL(tree.addContentItem(seriesUIDNode, DSRTypes::AM_afterCurrent),
                seriesUIDNode);
  }

  for(int i=0;i<measurements.size();i++){
    DSRNumTreeNode *measurementNode = new DSRNumTreeNode(DSRTypes::RT_contains);

    if(std::string(measurements[i].UnitsCode.getCodeValue().c_str()) == "{SUVbw}g/ml"){
      CHECK_COND(measurementNode->setConceptName(DSRCodedEntryValue("250122","99PMP","SUVbw")));
      DSRNumericMeasurementValue measurementValue(measurements[i].MeasurementValue.c_str(),
                                                  measurements[i].UnitsCode);
      CHECK_COND(measurementNode->setValue(measurementValue));
      CHECK_EQUAL(tree.addContentItem(measurementNode, DSRTypes::AM_afterCurrent),measurementNode);

      DSRCodeTreeNode *modNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
      CHECK_COND(modNode->setConceptName(DSRCodedEntryValue("121401","DCM","Derivation")));
      DSRCodedEntryValue quantityCode = measurements[i].QuantityCode;
      CHECK_COND(modNode->setCode(quantityCode.getCodeValue(), quantityCode.getCodingSchemeDesignator(), quantityCode.getCodeMeaning()));
      CHECK_EQUAL(tree.addContentItem(modNode, DSRTypes::AM_belowCurrent), modNode);
      tree.goUp();
      //std::cout << "Encoding only one measurement!" << std::endl;
      //break;
    } else {
      DSRCodedEntryValue quantityCode = measurements[i].QuantityCode;
      CHECK_COND(measurementNode->setConceptName(quantityCode));
                 //Code(quantityCode.getCodeValue(), quantityCode.getCodingSchemeDesignator(), quantityCode.getCodeMeaning()));
      DSRNumericMeasurementValue measurementValue(measurements[i].MeasurementValue.c_str(),
                                                  measurements[i].UnitsCode);
      measurementNode->setValue(measurementValue);
      CHECK_EQUAL(tree.addContentItem(measurementNode, DSRTypes::AM_afterCurrent),measurementNode);

      DSRCodeTreeNode *modNode = new DSRCodeTreeNode(DSRTypes::RT_hasConceptMod);
      CHECK_COND(modNode->setConceptName(DSRCodedEntryValue("G-C036","SRT","Measurement Method")));
      CHECK_COND(modNode->setCode("250132", "99PMP","SUV body weight calculation method"));
      CHECK_EQUAL(tree.addContentItem(modNode, DSRTypes::AM_belowCurrent), modNode);
      tree.goUp();
    }
    {
      DcmElement *e;
      char* rwvmInstanceUIDPtr;
      CHECK_COND(rwvmDataset->findAndGetElement(DCM_SOPInstanceUID, e));
      e->getString(rwvmInstanceUIDPtr);
      DSRCompositeTreeNode *rwvmNode = new DSRCompositeTreeNode(DSRTypes::RT_inferredFrom);
      DSRCompositeReferenceValue refValue(UID_RealWorldValueMappingStorage, rwvmInstanceUIDPtr);
      rwvmNode->setValue(refValue);
      CHECK_COND(rwvmNode->setConceptName(DSRCodedEntryValue("250201","99PMP","Real World Value Map used for measurements")));
      CHECK_EQUAL(tree.addContentItem(rwvmNode, DSRTypes::AM_belowCurrent), rwvmNode);
      tree.goUp();
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


#if 0

  char* segFileName = argv[1];

  char* imageFileName = argv[2];

  std::vector<std::string> referencedImages;
  for(int i=2;i<argc;i++){
    referencedImages.push_back(argv[i]);
  }

  DcmFileFormat *fileformatSR = new DcmFileFormat();
  DcmFileFormat *fileformatSEG = new DcmFileFormat();
  DcmFileFormat *fileformatImage = new DcmFileFormat();

  DcmElement *e;

  std::vector<std::string> referencedClassUIDs, referencedInstanceUIDs;
  std::string referencedStudyInstanceUID;

  // read the image
  fileformatImage->loadFile(imageFileName);
  DcmDataset *datasetImage = fileformatImage->getDataset();
  char *imageSeriesInstanceUIDPtr;
  datasetImage->findAndGetElement(DCM_SeriesInstanceUID,
                                  e);
  e->getString(imageSeriesInstanceUIDPtr);

  // read SEG and find out the study, series and instance UIDs
  //  of the source images used for segmentation
  fileformatSEG->loadFile(segFileName);
  DcmDataset *datasetSEG = fileformatSEG->getDataset();
  if(!getReferencedInstances(datasetSEG, referencedClassUIDs, referencedInstanceUIDs)){
      std::cerr << "Failed to find references to the source image" << std::endl;
      return -1;
  }

  char* segInstanceUIDPtr;
  datasetSEG->findAndGetElement(DCM_SOPInstanceUID, e);
  e->getString(segInstanceUIDPtr);

  DcmDataset *datasetSR = fileformatSR->getDataset();

  /*
   * Comprehensive SR IOD Modules
   *
   * Patient:
   *            Patient                     M
   *            Clinical Trial Subject      U
   * Study:
   *            General Study               M
   *            Patient Study               U
   *            Clinical Trial Study        U
   * Series:
   *            SR Document Series          M
   *            Clinical Trial Series       U
   * Frame of reference:
   *            Frame of Reference          U
   *            Synchronization             U
   * Equipment:
   *            General Equipment           M
   * Document:
   *            SR Document General         M
   *            SR Document Content         M
   *            SOP Common                  M
   */

  OFString reportUID;
  OFStatus status;

  DSRDocument *doc = new DSRDocument();

  size_t node;
  
  //

  // TODO: IF SOMETHING DOESN'T WORK - CHECK THE RETURN VALUE!!!

  // create root document
  doc->createNewDocument(DSRTypes::DT_ComprehensiveSR);
  doc->setSeriesDescription("ROI quantitative measurement");

  // add incofmation about the template used - see CP-452
  //  ftp://medical.nema.org/medical/dicom/final/cp452_ft.pdf
  node = doc->getTree().addContentItem(DSRTypes::RT_isRoot, DSRTypes::VT_Container);
  doc->getTree().getCurrentContentItem().setTemplateIdentification(
              "1000", "99QIICR");

  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("10001","99QIICR", "Quantitative measurement report"));

  // TID1204: Language of content item and descendants
  dcmHelpersCommon::addLanguageOfContent(doc);

  // TID 1001: Observation context
  //dcmHelpersCommon::addObservationContext(doc);
  // TODO: replace device observer name with git repository, and model name with the git hash
  dcmHelpersCommon::addObserverContext(doc, QIICR_DEVICE_OBSERVER_UID, "tid1411test",
                                      "QIICR", "0.0.1", "0");

  // TID 4020: Image library
  //  at the same time, add all referenced instances to CurrentRequestedProcedureEvidence sequence
  node = doc->getTree().addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("111028", "DCM", "Image Library"));
  for(int i=0;i<referencedImages.size();i++){
    DcmFileFormat *fileFormat = new DcmFileFormat();
    fileFormat->loadFile(referencedImages[i].c_str());    
    dcmHelpersCommon::addImageLibraryEntry(doc, fileFormat->getDataset());

    doc->getCurrentRequestedProcedureEvidence().addItem(*fileFormat->getDataset());
  }
  doc->getCurrentRequestedProcedureEvidence().addItem(*datasetSEG);


  WARN_IF_ERROR(doc->getTree().addContentItem(DSRTypes::RT_contains,
                                              DSRTypes::VT_Container,
                                              DSRTypes::AM_afterCurrent),
                "Findings container");
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121070","DCM","Findings"));

  // TID 1411
  doc->getTree().addContentItem(DSRTypes::RT_contains, DSRTypes::VT_Container, DSRTypes::AM_belowCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(DSRCodedEntryValue("125007","DCM","Measurement Group"));

  //
  node = doc->getTree().addContentItem(
              DSRTypes::RT_hasObsContext, DSRTypes::VT_Text, DSRTypes::AM_belowCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("112039","DCM","Tracking Identifier"));
  doc->getTree().getCurrentContentItem().setStringValue("Object1");

  //
  node = doc->getTree().addContentItem(
              DSRTypes::RT_hasObsContext, DSRTypes::VT_UIDRef, DSRTypes::AM_afterCurrent);

  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("112040","DCM","Tracking Unique Identifier"));
  char trackingUID[128];
  dcmGenerateUniqueIdentifier(trackingUID, SITE_INSTANCE_UID_ROOT);
  doc->getTree().getCurrentContentItem().setStringValue(trackingUID);

  node = doc->getTree().addContentItem(
              DSRTypes::RT_contains, DSRTypes::VT_Image, DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121191","DCM","Referenced Segment"));

  DSRImageReferenceValue segReference = DSRImageReferenceValue(UID_SegmentationStorage, segInstanceUIDPtr);
  segReference.getSegmentList().addItem(1);
  if(doc->getTree().getCurrentContentItem().setImageReference(segReference).bad()){
    std::cerr << "Failed to set segmentation image reference" << std::endl;
  }

  // Referenced series used for segmentation is not stored in the
  // segmentation object, so need to reference all images instead.
  // Can initialize if the source images are available.
  for(int i=0;i<referencedInstanceUIDs.size();i++){
    node = doc->getTree().addContentItem(
                DSRTypes::RT_contains, DSRTypes::VT_Image,
                DSRTypes::AM_afterCurrent);
    doc->getTree().getCurrentContentItem().setConceptName(
                DSRCodedEntryValue("121233","DCM","Source image for segmentation"));
    DSRImageReferenceValue imageReference =
            DSRImageReferenceValue(referencedClassUIDs[i].c_str(),referencedInstanceUIDs[i].c_str());
    if(doc->getTree().getCurrentContentItem().setImageReference(imageReference).bad()){
      std::cerr << "Failed to set source image reference" << std::endl;
    }
  }

  // Measurement container: TID 1419
  DSRNumericMeasurementValue measurement =
    DSRNumericMeasurementValue("70.978",
    DSRCodedEntryValue("[hnsf'U]","UCUM","Hounsfield unit"));

  doc->getTree().addContentItem(DSRTypes::RT_contains,
                                DSRTypes::VT_Num,
                                DSRTypes::AM_afterCurrent);
  doc->getTree().getCurrentContentItem().setNumericValue(measurement);

  doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("112031","DCM","Attenuation Coefficient"));

  WARN_IF_ERROR(doc->getTree().addContentItem(DSRTypes::RT_hasConceptMod,
                                DSRTypes::VT_Code,
                                DSRTypes::AM_belowCurrent), "Failed to add code");
  assert(doc->getTree().getCurrentContentItem().setConceptName(
              DSRCodedEntryValue("121401","DCM","Derivation")).good());
  assert(doc->getTree().getCurrentContentItem().setCodeValue(
              DSRCodedEntryValue("R-00317","SRT","Mean")).good());

  OFString contentDate, contentTime;
  DcmDate::getCurrentDate(contentDate);
  DcmTime::getCurrentTime(contentTime);

  // Note: ContentDate/Time are populated by DSRDocument
  doc->setSeriesDate(contentDate.c_str());
  doc->setSeriesTime(contentTime.c_str());

  doc->getCodingSchemeIdentification().addItem("99QIICR");
  doc->getCodingSchemeIdentification().setCodingSchemeUID(QIICR_CODING_SCHEME_UID_ROOT);
  doc->getCodingSchemeIdentification().setCodingSchemeName("QIICR Coding Scheme");
  doc->getCodingSchemeIdentification().setCodingSchemeResponsibleOrganization("Quantitative Imaging for Cancer Research, http://qiicr.org");

  doc->write(*datasetSR);

  dcmHelpersCommon::copyPatientModule(datasetImage,datasetSR);
  dcmHelpersCommon::copyPatientStudyModule(datasetImage,datasetSR);
  dcmHelpersCommon::copyGeneralStudyModule(datasetImage,datasetSR);

  fileformatSR->saveFile("report.dcm", EXS_LittleEndianExplicit);



  return 0;
}


int getReferencedInstances(DcmDataset* dataset,
                            std::vector<std::string> &classUIDs,
                            std::vector<std::string> &instanceUIDs){
  DcmItem *item, *subitem1, *subitem2;

  if(dataset->findAndGetSequenceItem(DCM_SharedFunctionalGroupsSequence, item).bad()){
    std::cerr << "Input segmentation does not contain SharedFunctionalGroupsSequence" << std::endl;
    return 0;
  }
  if(item->findAndGetSequenceItem(DCM_DerivationImageSequence, subitem1).bad()){
      std::cerr << "Input segmentation does not contain DerivationImageSequence" << std::endl;
      return 0;
  }
  if(subitem1->findAndGetSequenceItem(DCM_SourceImageSequence, subitem2).bad()){
      std::cerr << "Input segmentation does not contain SourceImageSequence" << std::endl;
      return 0;
  }

  int itemNumber = 0;
  while(subitem1->findAndGetSequenceItem(DCM_SourceImageSequence, subitem2, itemNumber++).good()){
    DcmElement *e;
    char *str;
    subitem2->findAndGetElement(DCM_ReferencedSOPClassUID, e);
    e->getString(str);
    classUIDs.push_back(str);

    subitem2->findAndGetElement(DCM_ReferencedSOPInstanceUID, e);
    e->getString(str);
    instanceUIDs.push_back(str);
  }

  return classUIDs.size();
}

#endif
