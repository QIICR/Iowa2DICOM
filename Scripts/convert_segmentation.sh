# This is a very basic initial version of the conversion script for single time
# point of Iowa QIN use case dataset. Prerequisites are that there is a
# directory called PET containing the PET series, and directory called Regions
# containing segmentations of the regions called tumor.nrrd,
# aorta_resampled.nrrd, liver.nrrd and cerebellum.nrrd.
#
# The script will create the derived series in the following directories:
#   RWVM/SUVbw.dcm
#   

SUV_CALCULATOR=/Users/fedorov/local/builds/Slicer-SUVFactorCalculator/lib/Slicer-4.4/cli-modules/SUVFactorCalculator
INDEX_CALCULATOR=/Users/fedorov/local/builds/Slicer-QuantitativeIndicesExt/lib/Slicer-4.4/cli-modules/QuantitativeIndicesCLI
SEG_CONVERTER=/Users/fedorov/local/builds/Slicer4-Debug/Slicer-build/lib/Slicer-4.4/cli-modules/EncodeSEG
SR_CONVERTER=/Users/fedorov/local/builds/Slicer4-Debug/Slicer-build/lib/Slicer-4.4/cli-modules/EncodeMeasurementsSR

SLICER_BINARY=/Applications/Slicer-2014-12-12-nightly.app/Contents/MacOS/Slicer

DATA=/Users/fedorov/Documents/Projects/QIICR/Iowa_patient62_PET_1tp/NAMIC2015

# Step 4: encode each of the segmentations in DICOM
# EncodeSEG
mkdir $DATA/SEG
rm -rf $DATA/SEG/*
$SEG_CONVERTER --skip --readerId 1 --sessionId 1 --timepointId 1 --seriesDescription "Cerebellum segmentation" --segImageFiles  $DATA/Regions/tumor.nrrd --labelAttributesFiles $DATA/Regions/tumor.info --segDICOMFile $DATA/SEG/tumor.dcm --dicomImageFiles $DATA/PET/0.dcm,$DATA/PET/1.dcm,$DATA/PET/10.dcm,$DATA/PET/100.dcm,$DATA/PET/101.dcm,$DATA/PET/102.dcm,$DATA/PET/103.dcm,$DATA/PET/104.dcm,$DATA/PET/105.dcm,$DATA/PET/106.dcm,$DATA/PET/107.dcm,$DATA/PET/108.dcm,$DATA/PET/109.dcm,$DATA/PET/11.dcm,$DATA/PET/110.dcm,$DATA/PET/111.dcm,$DATA/PET/112.dcm,$DATA/PET/113.dcm,$DATA/PET/114.dcm,$DATA/PET/115.dcm,$DATA/PET/116.dcm,$DATA/PET/117.dcm,$DATA/PET/118.dcm,$DATA/PET/119.dcm,$DATA/PET/12.dcm,$DATA/PET/120.dcm,$DATA/PET/121.dcm,$DATA/PET/122.dcm,$DATA/PET/123.dcm,$DATA/PET/124.dcm,$DATA/PET/125.dcm,$DATA/PET/126.dcm,$DATA/PET/127.dcm,$DATA/PET/128.dcm,$DATA/PET/129.dcm,$DATA/PET/13.dcm,$DATA/PET/130.dcm,$DATA/PET/131.dcm,$DATA/PET/132.dcm,$DATA/PET/133.dcm,$DATA/PET/134.dcm,$DATA/PET/135.dcm,$DATA/PET/136.dcm,$DATA/PET/137.dcm,$DATA/PET/138.dcm,$DATA/PET/139.dcm,$DATA/PET/14.dcm,$DATA/PET/140.dcm,$DATA/PET/141.dcm,$DATA/PET/142.dcm,$DATA/PET/143.dcm,$DATA/PET/144.dcm,$DATA/PET/145.dcm,$DATA/PET/146.dcm,$DATA/PET/147.dcm,$DATA/PET/148.dcm,$DATA/PET/149.dcm,$DATA/PET/15.dcm,$DATA/PET/150.dcm,$DATA/PET/151.dcm,$DATA/PET/152.dcm,$DATA/PET/153.dcm,$DATA/PET/154.dcm,$DATA/PET/155.dcm,$DATA/PET/156.dcm,$DATA/PET/157.dcm,$DATA/PET/158.dcm,$DATA/PET/159.dcm,$DATA/PET/16.dcm,$DATA/PET/160.dcm,$DATA/PET/161.dcm,$DATA/PET/162.dcm,$DATA/PET/163.dcm,$DATA/PET/164.dcm,$DATA/PET/165.dcm,$DATA/PET/166.dcm,$DATA/PET/167.dcm,$DATA/PET/168.dcm,$DATA/PET/169.dcm,$DATA/PET/17.dcm,$DATA/PET/170.dcm,$DATA/PET/171.dcm,$DATA/PET/172.dcm,$DATA/PET/173.dcm,$DATA/PET/174.dcm,$DATA/PET/175.dcm,$DATA/PET/176.dcm,$DATA/PET/177.dcm,$DATA/PET/178.dcm,$DATA/PET/179.dcm,$DATA/PET/18.dcm,$DATA/PET/180.dcm,$DATA/PET/181.dcm,$DATA/PET/182.dcm,$DATA/PET/183.dcm,$DATA/PET/184.dcm,$DATA/PET/185.dcm,$DATA/PET/186.dcm,$DATA/PET/187.dcm,$DATA/PET/188.dcm,$DATA/PET/189.dcm,$DATA/PET/19.dcm,$DATA/PET/190.dcm,$DATA/PET/191.dcm,$DATA/PET/192.dcm,$DATA/PET/193.dcm,$DATA/PET/194.dcm,$DATA/PET/195.dcm,$DATA/PET/196.dcm,$DATA/PET/197.dcm,$DATA/PET/198.dcm,$DATA/PET/199.dcm,$DATA/PET/2.dcm,$DATA/PET/20.dcm,$DATA/PET/200.dcm,$DATA/PET/201.dcm,$DATA/PET/202.dcm,$DATA/PET/203.dcm,$DATA/PET/204.dcm,$DATA/PET/205.dcm,$DATA/PET/206.dcm,$DATA/PET/207.dcm,$DATA/PET/208.dcm,$DATA/PET/209.dcm,$DATA/PET/21.dcm,$DATA/PET/210.dcm,$DATA/PET/211.dcm,$DATA/PET/212.dcm,$DATA/PET/213.dcm,$DATA/PET/214.dcm,$DATA/PET/215.dcm,$DATA/PET/216.dcm,$DATA/PET/217.dcm,$DATA/PET/218.dcm,$DATA/PET/219.dcm,$DATA/PET/22.dcm,$DATA/PET/220.dcm,$DATA/PET/221.dcm,$DATA/PET/222.dcm,$DATA/PET/223.dcm,$DATA/PET/224.dcm,$DATA/PET/225.dcm,$DATA/PET/226.dcm,$DATA/PET/227.dcm,$DATA/PET/228.dcm,$DATA/PET/229.dcm,$DATA/PET/23.dcm,$DATA/PET/230.dcm,$DATA/PET/231.dcm,$DATA/PET/232.dcm,$DATA/PET/233.dcm,$DATA/PET/234.dcm,$DATA/PET/235.dcm,$DATA/PET/236.dcm,$DATA/PET/237.dcm,$DATA/PET/238.dcm,$DATA/PET/239.dcm,$DATA/PET/24.dcm,$DATA/PET/240.dcm,$DATA/PET/241.dcm,$DATA/PET/242.dcm,$DATA/PET/243.dcm,$DATA/PET/244.dcm,$DATA/PET/245.dcm,$DATA/PET/246.dcm,$DATA/PET/247.dcm,$DATA/PET/248.dcm,$DATA/PET/249.dcm,$DATA/PET/25.dcm,$DATA/PET/250.dcm,$DATA/PET/251.dcm,$DATA/PET/252.dcm,$DATA/PET/253.dcm,$DATA/PET/254.dcm,$DATA/PET/255.dcm,$DATA/PET/256.dcm,$DATA/PET/257.dcm,$DATA/PET/258.dcm,$DATA/PET/259.dcm,$DATA/PET/26.dcm,$DATA/PET/260.dcm,$DATA/PET/261.dcm,$DATA/PET/262.dcm,$DATA/PET/263.dcm,$DATA/PET/264.dcm,$DATA/PET/265.dcm,$DATA/PET/266.dcm,$DATA/PET/267.dcm,$DATA/PET/268.dcm,$DATA/PET/269.dcm,$DATA/PET/27.dcm,$DATA/PET/270.dcm,$DATA/PET/271.dcm,$DATA/PET/272.dcm,$DATA/PET/273.dcm,$DATA/PET/274.dcm,$DATA/PET/275.dcm,$DATA/PET/276.dcm,$DATA/PET/277.dcm,$DATA/PET/278.dcm,$DATA/PET/279.dcm,$DATA/PET/28.dcm,$DATA/PET/280.dcm,$DATA/PET/281.dcm,$DATA/PET/282.dcm,$DATA/PET/283.dcm,$DATA/PET/284.dcm,$DATA/PET/285.dcm,$DATA/PET/286.dcm,$DATA/PET/287.dcm,$DATA/PET/288.dcm,$DATA/PET/289.dcm,$DATA/PET/29.dcm,$DATA/PET/290.dcm,$DATA/PET/291.dcm,$DATA/PET/292.dcm,$DATA/PET/293.dcm,$DATA/PET/294.dcm,$DATA/PET/295.dcm,$DATA/PET/296.dcm,$DATA/PET/297.dcm,$DATA/PET/298.dcm,$DATA/PET/3.dcm,$DATA/PET/30.dcm,$DATA/PET/31.dcm,$DATA/PET/32.dcm,$DATA/PET/33.dcm,$DATA/PET/34.dcm,$DATA/PET/35.dcm,$DATA/PET/36.dcm,$DATA/PET/37.dcm,$DATA/PET/38.dcm,$DATA/PET/39.dcm,$DATA/PET/4.dcm,$DATA/PET/40.dcm,$DATA/PET/41.dcm,$DATA/PET/42.dcm,$DATA/PET/43.dcm,$DATA/PET/44.dcm,$DATA/PET/45.dcm,$DATA/PET/46.dcm,$DATA/PET/47.dcm,$DATA/PET/48.dcm,$DATA/PET/49.dcm,$DATA/PET/5.dcm,$DATA/PET/50.dcm,$DATA/PET/51.dcm,$DATA/PET/52.dcm,$DATA/PET/53.dcm,$DATA/PET/54.dcm,$DATA/PET/55.dcm,$DATA/PET/56.dcm,$DATA/PET/57.dcm,$DATA/PET/58.dcm,$DATA/PET/59.dcm,$DATA/PET/6.dcm,$DATA/PET/60.dcm,$DATA/PET/61.dcm,$DATA/PET/62.dcm,$DATA/PET/63.dcm,$DATA/PET/64.dcm,$DATA/PET/65.dcm,$DATA/PET/66.dcm,$DATA/PET/67.dcm,$DATA/PET/68.dcm,$DATA/PET/69.dcm,$DATA/PET/7.dcm,$DATA/PET/70.dcm,$DATA/PET/71.dcm,$DATA/PET/72.dcm,$DATA/PET/73.dcm,$DATA/PET/74.dcm,$DATA/PET/75.dcm,$DATA/PET/76.dcm,$DATA/PET/77.dcm,$DATA/PET/78.dcm,$DATA/PET/79.dcm,$DATA/PET/8.dcm,$DATA/PET/80.dcm,$DATA/PET/81.dcm,$DATA/PET/82.dcm,$DATA/PET/83.dcm,$DATA/PET/84.dcm,$DATA/PET/85.dcm,$DATA/PET/86.dcm,$DATA/PET/87.dcm,$DATA/PET/88.dcm,$DATA/PET/89.dcm,$DATA/PET/9.dcm,$DATA/PET/90.dcm,$DATA/PET/91.dcm,$DATA/PET/92.dcm,$DATA/PET/93.dcm,$DATA/PET/94.dcm,$DATA/PET/95.dcm,$DATA/PET/96.dcm,$DATA/PET/97.dcm,$DATA/PET/98.dcm,$DATA/PET/99.dcm
#echo $CMD
$CMD
