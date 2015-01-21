# This is a very basic initial version of the conversion script for single time
# point of Iowa QIN use case dataset. Prerequisites are that there is a
# directory called PET containing the PET series, and directory called Regions
# containing segmentations of the regions called tumor.nrrd,
# aorta_resampled.nrrd, liver.nrrd and cerebellum.nrrd.
#
# The script will create the derived series in the following directories:
#   RWVM/SUVbw.dcm
#   

SUV_CALCULATOR=/Users/ejulrich/software/develop/Slicer-Extensions/PETDICOMExtension/build/lib/Slicer-4.4/cli-modules/SUVFactorCalculator
INDEX_CALCULATOR=/Users/ejulrich/software/develop/Slicer-Extensions/QuantitativeIndicesExt/build/lib/Slicer-4.4/cli-modules/QuantitativeIndicesCLI
SEG_CONVERTER=/Users/ejulrich/software/Slicer-SuperBuild-Debug/Slicer-build/lib/Slicer-4.4/cli-modules/EncodeSEG
SR_CONVERTER=/Users/ejulrich/software/Slicer-SuperBuild-Debug/Slicer-build/lib/Slicer-4.4/cli-modules/EncodeMeasurementsSR
SEG_COUNTER=/Users/ejulrich/software/Slicer-SuperBuild-Debug/Slicer-build/lib/Slicer-4.4/cli-modules/CountSegmentations

SLICER_BINARY=/Users/ejulrich/software/Slicer-SuperBuild-Debug/Slicer-build/bin/Slicer.app/Contents/MacOS/Slicer

DATA=/Users/ejulrich/data/Iowa_sample_data-Patient62

# Step 1: calculate SUVbw factor, save SUV corrected image and RWVM object
mkdir $DATA/RWVM
rm -rf $DATA/RWVM/*
$SUV_CALCULATOR --petDICOMPath $DATA/PET --rwvmDICOMPath $DATA/RWVM --returnparameterfile $DATA/Derived/suv.txt

rwvmName=$DATA/RWVM/`ls ${DATA}/RWVM`

# Apply SUV correction to the PET series
mkdir $DATA/SUV
rm -rf $DATA/SUV/*
$SLICER_BINARY --no-splash --disable-cli-modules --no-main-window --python-script Scripts/apply_rwvm.py $DATA/PET $DATA/RWVM $DATA/SUV

# Step 2: Find all label values in 'tumor' label, as these will be
# case-specific
seg_counter_output="$($SEG_COUNTER --segImageFile $DATA/Regions/tumor.nrrd --returnparameterfile $DATA/Derived/out.txt --outputFile ~/data/Iowa_sample_data-Patient62/out.csv)"
seg_labels=($seg_counter_output)

# Step 3: calculate quantitative indices for each of the regions
#  For tumor, do it separately for each label found
mkdir $DATA/Indices
TUMOR_MEASUREMENT_FILES=''
rm -rf $DATA/Indices/*
for region in 'tumor' 'aorta_resampled' 'liver' 'cerebellum'
do
  if [ $region == 'tumor' ]
  then
    for l in "${seg_labels[@]}"
    do
      if [ $l == '1' ]
      then
        TUMOR_MEASUREMENT_FILES="${TUMOR_MEASUREMENT_FILES}$DATA/Indices/tumor.txt,"
        $INDEX_CALCULATOR \
        --peak --sambg --sam --q4 --q3 --q2 --q1 --gly4 --gly3 --gly2 --gly1 --tlg --quart3 --median \
        --quart1 --volume --rms --max --min --variance --mean \
        --returnparameterfile $DATA/Indices/tumor.txt --adj $DATA/SUV/SUVbw.nrrd $DATA/Regions/tumor.nrrd 1
      else
        TUMOR_MEASUREMENT_FILES="${TUMOR_MEASUREMENT_FILES}$DATA/Indices/lymph_node${l}.txt,"
        $INDEX_CALCULATOR \
        --peak --sambg --sam --q4 --q3 --q2 --q1 --gly4 --gly3 --gly2 --gly1 --tlg --quart3 --median \
        --quart1 --volume --rms --max --min --variance --mean \
        --returnparameterfile $DATA/Indices/lymph_node${l}.txt --adj $DATA/SUV/SUVbw.nrrd $DATA/Regions/tumor.nrrd $l
      fi
    done
  else
    $INDEX_CALCULATOR \
    --peak --sambg --sam --q4 --q3 --q2 --q1 --gly4 --gly3 --gly2 --gly1 --tlg --quart3 --median \
    --quart1 --volume --rms --max --min --variance --mean \
    --returnparameterfile $DATA/Indices/${region}.txt --adj $DATA/SUV/SUVbw.nrrd $DATA/Regions/${region}.nrrd 1
  fi
done
TUMOR_MEASUREMENT_FILES=${TUMOR_MEASUREMENT_FILES%?} # strip last comma

# Step 4: encode each of the segmentations in DICOM
# EncodeSEG
mkdir $DATA/SEG
rm -rf $DATA/SEG/*
PET_FILES=`ls -d1 -m ${DATA}/PET/*`
PET_FILES=`echo $PET_FILES | tr -d ' '` # remove spaces
$SEG_CONVERTER --segImageFiles  $DATA/Regions/tumor.nrrd,$DATA/Regions/cerebellum.nrrd,$DATA/Regions/aorta_resampled.nrrd,$DATA/Regions/liver.nrrd --segDICOMFile $DATA/SEG/combined.dcm --dicomImageFiles $PET_FILES

# Step 5: encode SR - work in progress
mkdir $DATA/SR
rm -rf $DATA/SR/*
CMD="$SR_CONVERTER --petFiles $PET_FILES --segFile $DATA/SEG/combined.dcm --rwvmFile $rwvmName --measurementFiles $TUMOR_MEASUREMENT_FILES,$DATA/Indices/aorta_resampled.txt,$DATA/Indices/cerebellum.txt,$DATA/Indices/liver.txt  --quantityTerms Resources/QuantitiesAndUnits.csv --srFile $DATA/SR/measurements.dcm"
echo $CMD
$CMD
