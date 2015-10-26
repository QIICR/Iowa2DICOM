#!/usr/bin/python
import os
import csv
import sys
import glob
from QINSQLDatabase import QINSQLDatabase
from run2 import selectReferenceRegionSegmentation

if len(sys.argv) != 4:
  sys.exit("USAGE: encode_all_objects.py QIN_DICOM_DIR {tcia or xnat} {0=keep old, 1=delete old}")
  
db = sys.argv[2]
if db not in ['tcia','xnat']:
  sys.exit("ERROR: first argument must be tcia or xnat")
  
clean = int(sys.argv[3])
if clean not in [0,1]:
  sys.exit("USAGE: encode_all_objects.py QIN_DICOM_DIR {tcia or xnat} {0=keep old, 1=delete old}")

QIN_DICOM_DIR = sys.argv[1]
executablesDirectory = QIN_DICOM_DIR +"/executables/"
SUV_CALCULATOR = executablesDirectory + "SUVFactorCalculator"
SLICER_BINARY = executablesDirectory + "Slicer"
INDEX_CALCULATOR = executablesDirectory + "QuantitativeIndicesCLI"
SEG_CONVERTER = executablesDirectory + "EncodeSEG"
SR_CONVERTER = executablesDirectory + "EncodeMeasurementsSR"

# logger
logFile = QIN_DICOM_DIR +"/log/log.txt"
os.system("touch " + logFile)
logger = csv.writer(open(logFile, 'wb'))

# get dictionary of patients from the study
dataset_list = []
sqldb = QINSQLDatabase()
cursor = sqldb.getCursor()
query = "select subject.full_s_label, scan.c_datetime, p.fc_site, p.fc_site_category from icts_pacs_subject subject, icts_pacs_scan scan, patient p where subject.id=scan.icts_subject_id and p.id=subject.patient_id and scan.xnat_project_name='U01' and scan.id in (select pacs_scan_id from image_annotation where qin_user_id=15 and algorithm_id in (13) and priority>=1.0) order by subject.full_s_label, scan.c_datetime;"
cursor.execute(query)
query_results = cursor.fetchall()
for query_result in query_results:
  full_s_label = query_result[0]
  c_datetime = query_result[1]
  dataset = {'full_s_label':full_s_label, 'c_datetime':c_datetime}
  dataset['directory'] = os.path.join(QIN_DICOM_DIR +'/patients',dataset['full_s_label']+"_"+dataset['c_datetime'])
  dataset_list.append(dataset)
  
# dictionary for device UIDs
deviceUID = {'aorta':'2.25.306661657999604669424834320660963473324', 
             'cerebellum':'2.25.308434430126787466538588791128025151404',
             'liver':'2.25.309174254785529415496603545181867685804'}
# dictionary for device names
deviceName = {'aorta':'Automated Aortic Arch Reference Region Segmentation', 
              'cerebellum':'Automated Cerebellum Reference Region Segmentation',
              'liver':'Automated Liver Reference Region Segmentation'}

seriesNumber = 1

print dataset_list
 
for dataset in dataset_list:
  patientLabel = dataset['full_s_label'] +"_"+ dataset['c_datetime']
  logger.writerow([patientLabel +":"])

  # directories
  PATIENT_DIR = QIN_DICOM_DIR +"/patients/"+ patientLabel +"/"
  DICOM_DIR = PATIENT_DIR + "DICOM/"
  VOLUME_DIR = PATIENT_DIR + "volumes/"
  INFO_DIR = PATIENT_DIR + "info/"
  INDICES_DIR = PATIENT_DIR + "indices/"
  
  # PET/CT files
  PET_DIR = PATIENT_DIR + db + "_PT/"
  petFiles = ''
  petFileList = os.listdir(PET_DIR)
  for petFile in petFileList:
    petFiles = petFiles + PET_DIR + petFile +","
  petFiles = petFiles[:-1]

  CT_DIR = PATIENT_DIR + db + "_CT/"
  ctFiles = ''
  ctFileList = os.listdir(CT_DIR)
  for ctFile in ctFileList:
    ctFiles = ctFiles + CT_DIR + ctFile +","
  ctFiles = ctFiles[:-1]
    
  os.system("mkdir "+ DICOM_DIR +"RWVM")
  os.system("mkdir "+ DICOM_DIR +"SEG")
  os.system("mkdir "+ DICOM_DIR +"SR")
  if clean:
    os.system("rm -r "+ DICOM_DIR +"RWVM/*")
    os.system("rm -r "+ DICOM_DIR +"SEG/*")
    os.system("rm -r "+ DICOM_DIR +"SR/*")
    os.system("rm -r "+ INDICES_DIR +"*")
  
  timepoint = ''
  if dataset['full_s_label']=='IA018_000047_KR' and dataset['c_datetime']=='2006-08-10': # special case
    timepoint = '2'
  else:
    timepoint = '1'
  logger.writerow(["  timepoint: "+ timepoint])
  
  # Calculate SUVbw factor, save RWVM object and SUV corrected image
  os.system(SUV_CALCULATOR +" --petDICOMPath "+ PET_DIR +" --rwvmDICOMPath "+ DICOM_DIR +"RWVM --returnparameterfile "+ INFO_DIR +"suv.txt --seriesNumber "+ str(seriesNumber))
  seriesNumber = seriesNumber + 1
  RWVM = ''
  rwvmList = os.listdir(DICOM_DIR +"RWVM")
  if len(rwvmList)==0:
    print("ERROR: could not generate RWVM for "+ patientLabel)
    logger.writerow(["  ERROR: could not generate RWVM for "+ patientLabel])
    continue
  elif len(rwvmList)>1:
    sys.exit("ERROR: more than one RWVM detected for "+ patientLabel +"!")
  else:
    RWVM = DICOM_DIR +"RWVM/"+ rwvmList[0]
    logger.writerow(["  RWVM (SeriesNumber "+ str(seriesNumber-1) +"): "+ RWVM])
  os.system("dcmodify -i BodyPartExamined=HEADNECK "+ RWVM) # hack for now
  
  # Reference Regions
  for region in ['aorta','cerebellum','liver']:
    logger.writerow(["  "+ region + ":"])
    segFile = ''
    segFile = selectReferenceRegionSegmentation(dataset,region)
      
    # Calculate quantitative indices
    indicesFile = INDICES_DIR + region +".csv"
    os.system(INDEX_CALCULATOR +" --returnCSV --csvFile "+ indicesFile +" --mean --stddev --quart1 --median --quart3 --min --max --volume --returnparameterfile "+ INDICES_DIR + region +".txt "+ VOLUME_DIR +"SUVbw.nrrd "+ segFile +" 1")
    if not os.path.isfile(indicesFile):
      print("ERROR: failed to calculate "+ region +" indices for "+ patientLabel)
      logger.writerow(["    ERROR: failed to calculate "+ region +" indices for "+ patientLabel])
      continue
    else:
      logger.writerow(["    Indices: " + indicesFile])
    os.system("rm "+ INDICES_DIR + region +".txt") # not used

    refMethod = ''
    readerId = ''
    if "auto" in segFile:
      refMethod = "auto"
      #readerId = "1"
    elif "manual" in segFile:
      refMethod = "manual"
      readerId = "User4"
      if patientLabel == "IA018_000157_NC_2004-03-02": # special case, manual cerebellum segmentation
        readerId = "User5"
    else:
      sys.exit("ERROR: unknown reference region segmentation for "+ patientLabel +"!")
    infoFile = INFO_DIR + region +"_"+ refMethod +".info"
    if not os.path.isfile(infoFile):
      print("ERROR: could not find "+ region +" info file for "+ patientLabel)
      logger.writerow(["    ERROR: could not find "+ region +" info file for "+ patientLabel])
      continue
    
    dicomImageFiles = ''  
    if region=='aorta':
      dicomImageFiles = ctFiles
    else:
      dicomImageFiles = petFiles
      
    # Encode Segmentations
    dcmSEG = DICOM_DIR +"SEG/"+ region +".dcm"
    if refMethod == "auto":
      os.system(SEG_CONVERTER +" --skip --readerId \""+ deviceName[region] +"\" --sessionId 1 --timepointId "+ timepoint +" --seriesNumber "+ str(seriesNumber) +" --bodyPart HEADNECK --seriesDescription \""+ refMethod +" "+ region +" segmentation\" --segImageFiles "+ segFile +" --labelAttributesFiles "+ infoFile +" --segDICOMFile "+ dcmSEG +" --dicomImageFiles "+ dicomImageFiles)
    elif refMethod == "manual":
      os.system(SEG_CONVERTER +" --skip --readerId "+ readerId +" --sessionId 1 --timepointId "+ timepoint +" --seriesNumber "+ str(seriesNumber) +" --bodyPart HEADNECK --seriesDescription \""+ refMethod +" "+ region +" segmentation\" --segImageFiles "+ segFile +" --labelAttributesFiles "+ infoFile +" --segDICOMFile "+ dcmSEG +" --dicomImageFiles "+ dicomImageFiles)
    else:
      sys.exit("ERROR: unknown reference region segmentation for "+ patientLabel +"!")
    seriesNumber = seriesNumber + 1
    if not os.path.isfile(dcmSEG):
      print("ERROR: failed to create "+ region +" SEG for "+ patientLabel)
      logger.writerow(["    ERROR: failed to create "+ region +" SEG for "+ patientLabel])
      continue
    else:
      logger.writerow(["    SEG (SeriesNumber "+ str(seriesNumber-1) +"): "+ dcmSEG])
    
    # Generate Structured Reports
    dcmSR = DICOM_DIR +"SR/"+ region +".dcm"
    if refMethod == "auto":
      os.system(SR_CONVERTER +" --deviceUID \""+ deviceUID[region] +"\" --deviceName \""+ deviceName[region] +"\" --sessionId 1 --timepointId "+ timepoint +" --seriesNumber "+ str(seriesNumber) +" --seriesDescription \"reference region measurements - "+ region +"\" --petFiles "+ petFiles +" --segFile "+ dcmSEG +" --rwvmFile "+ RWVM +" --measurementsFile "+ indicesFile +" --quantityTerms resources/QuantitiesAndUnits.csv --srFile "+ dcmSR +" --labelAttributes "+ infoFile)
    elif refMethod == "manual":
      os.system(SR_CONVERTER +" --readerId "+ readerId +" --sessionId 1 --timepointId "+ timepoint +" --seriesNumber "+ str(seriesNumber) +" --seriesDescription \"reference region measurements - "+ region +"\" --petFiles "+ petFiles +" --segFile "+ dcmSEG +" --rwvmFile "+ RWVM +" --measurementsFile "+ indicesFile +" --quantityTerms resources/QuantitiesAndUnits.csv --srFile "+ dcmSR +" --labelAttributes "+ infoFile)
    else:
      sys.exit("ERROR: unknown reference region segmentation for "+ patientLabel +"!")
    seriesNumber = seriesNumber + 1
    if not os.path.isfile(dcmSR):
      print("ERROR: failed to create "+ region +" SR for "+ patientLabel)
      logger.writerow(["    ERROR: failed to create "+ region +" SR for "+ patientLabel])
    else:
      logger.writerow(["    SR (SeriesNumber "+ str(seriesNumber-1) +"): "+ dcmSR])
    
  # Tumor segmentations { User | Method | Trial }
  for user in ['User1','User2','User3']:
    logger.writerow(["  "+ user + ":"])
    for method in ['Manual','SemiAuto']:
      for trial in ['1','2']:
        logger.writerow(["    "+ method +" Trial "+ trial +":"])
        segFile = VOLUME_DIR +"tumor_"+ user +"_"+ method +"_Trial"+ trial +".nii.gz"
        if not os.path.isfile(segFile):
          print("ERROR: could not find tumor segmentation for "+ patientLabel)
          logger.writerow(["      ERROR: could not find tumor segmentation for "+ patientLabel])
          continue
        infoFile = INFO_DIR + "tumor_"+ user +"_"+ method +"_Trial"+ trial +".info"
        if not os.path.isfile(infoFile):
          print("ERROR: could not find tumor info file for "+ patientLabel)
          logger.writerow(["      ERROR: could not find tumor info file for "+ patientLabel])
          continue

        # Calculate Quantitative Indices
        indicesFile = INDICES_DIR +"tumor_"+ user +"_"+ method +"_Trial"+ trial +".csv"
        os.system(INDEX_CALCULATOR +" --returnCSV --csvFile "+ indicesFile +" --peak --sambg --sam --q4 --q3 --q2 --q1 --gly4 --gly3 --quart1 --volume --rms --max --min --stddev --mean --gly2 --gly1 --tlg --quart3 --median --adj --returnparameterfile "+ INDICES_DIR +"tumor_"+ user +"_"+ method +"_Trial"+ trial +".txt "+ VOLUME_DIR +"SUVbw.nrrd "+ segFile +" 1")
        if not os.path.isfile(indicesFile):
          print("ERROR: failed to calculate tumor indices for "+ patientLabel)
          logger.writerow(["      ERROR: failed to calculate tumor indices for "+ patientLabel])
          continue
        else:
          logger.writerow(["      Indices: "+ indicesFile])
        os.system("rm "+ INDICES_DIR +"tumor_"+ user +"_"+ method +"_Trial"+ trial +".txt") # not used
          
        #Encode Segmentations
        dcmSEG = DICOM_DIR +"SEG/tumor_"+ user +"_"+ method +"_Trial"+ trial +".dcm"
        os.system(SEG_CONVERTER +" --skip --readerId "+ user +" --sessionId "+ trial +" --timepointId "+ timepoint +" --seriesNumber "+ str(seriesNumber) +" --bodyPart HEADNECK --seriesDescription \"tumor segmentation - "+ user +" "+ method +" trial "+ trial +"\" --segImageFiles "+ segFile +" --labelAttributesFiles "+ infoFile +" --segDICOMFile "+ dcmSEG +" --dicomImageFiles "+ petFiles)
        seriesNumber = seriesNumber + 1
        if not os.path.isfile(dcmSEG):
          print("ERROR: failed to create tumor SEG for "+ patientLabel)
          logger.writerow(["      ERROR: failed to create tumor SEG for "+ patientLabel])
          continue
        else:
          logger.writerow(["      SEG (SeriesNumber "+ str(seriesNumber-1) +"): "+ dcmSEG])
        
        # Generate Structured Reports
        dcmSR = DICOM_DIR +"SR/tumor_"+ user +"_"+ method +"_Trial"+ trial +".dcm"
        os.system(SR_CONVERTER +" --readerId "+ user +" --sessionId "+ trial +" --timepointId "+ timepoint +" --seriesNumber "+ str(seriesNumber) +" --seriesDescription \"tumor measurements - "+ user +" "+ method +" trial "+ trial +"\" --petFiles "+ petFiles +" --segFile "+ dcmSEG +" --rwvmFile "+ RWVM +" --measurementsFile "+ indicesFile +" --quantityTerms resources/QuantitiesAndUnits.csv --srFile "+ dcmSR +" --labelAttributes "+ infoFile)
        seriesNumber = seriesNumber + 1
        if not os.path.isfile(dcmSR):
          print("ERROR: failed to create tumor SR for "+ patientLabel)
          logger.writerow(["      ERROR: failed to create tumor SR for "+ patientLabel])
        else:
          logger.writerow(["      SR (SeriesNumber "+ str(seriesNumber-1) +"): "+ dcmSR])

  #seriesNumber = seriesNumber + 1
  logger.writerow([" "])
