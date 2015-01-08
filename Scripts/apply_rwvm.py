import sys, os, slicer, vtk

'''
 Usage:
  Slicer.exe --no-splash --no-main-window --python-script converter.py <directory with PET series> <directory with RWVM series> <output directory>

TODO:
  Use new directory and new database

'''

petDir = sys.argv[1]
rwvmDir = sys.argv[2]
outputDir = sys.argv[3]

print(petDir)
print(rwvmDir)
print(outputDir)

# get lists of DICOM and seg files with full path names
# DICOM files fill be sorted by the slice positions for volume reconstruction
rwvmList = []
for dcm in os.listdir(rwvmDir):
  if len(dcm)-dcm.rfind('.dcm') == 4:
    rwvmList.append(rwvmDir+'/'+dcm)

rwvmList = rwvmList[:1]

rwvmPlugin = slicer.modules.dicomPlugins['DICOMRWVMPlugin']()

# index input DICOM series
indexer = ctk.ctkDICOMIndexer()
indexer.addDirectory(slicer.dicomDatabase, petDir)
indexer.waitForImportFinished()
indexer.addDirectory(slicer.dicomDatabase, rwvmDir)
indexer.waitForImportFinished()

loadables = rwvmPlugin.examine([rwvmList])

if len(loadables) == 0:
  print 'Could not parse the DICOM Study!'
  exit()

# load input DICOM volume
dcmVolume = rwvmPlugin.load(loadables[0])

# save DICOM volume as single file nrrd format
dcmStorage = slicer.vtkMRMLVolumeArchetypeStorageNode()
dcmStorage.SetWriteFileFormat('nrrd')
dcmStorage.SetFileName(os.path.join(outputDir,'SUVbw.nrrd'))
dcmStorage.WriteData(dcmVolume)

sys.exit()
