/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

//
//  This example illustrates how to read a DICOM series into a volume
//  and then save this volume into another DICOM series using the
//  exact same name.
//  It makes use of the DCMTK library
//

#include "itkImageSeriesReader.h"
#include "itkImageSeriesWriter.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkDCMTKImageIO.h"
#include "itkDCMTKSeriesFileNames.h"

#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"

#include "DCMTKSeriesReadImageWriteCLP.h"

int main( int argc, char* argv[] )
{
  PARSE_ARGS;
  if( argc < 3 )
    {
    std::cerr << "Usage: " << argv[0] <<
      " DicomDirectory  outputFile OutputDicomDirectory" << std::endl;
    return EXIT_FAILURE;
    }

  typedef itk::Image<unsigned short,3>            ImageType;
  typedef itk::ImageSeriesReader< ImageType >     ReaderType;

  typedef itk::DCMTKImageIO                       DCMTKImageIOType;
  typedef itk::DCMTKSeriesFileNames               DCMTKSeriesFileNames;

  typedef itk::GDCMImageIO                       GDCMImageIOType;
  typedef itk::GDCMSeriesFileNames               GDCMSeriesFileNames;

  DCMTKImageIOType::Pointer dcmtkIO = DCMTKImageIOType::New();
  GDCMImageIOType::Pointer gdcmIO = GDCMImageIOType::New();
  DCMTKSeriesFileNames::Pointer it = DCMTKSeriesFileNames::New();

  typedef itk::ImageFileWriter< ImageType > WriterType;
  WriterType::Pointer writer = WriterType::New();

  //it->SetInputDirectory( argv[1] );

  ReaderType::Pointer reader = ReaderType::New();

  ReaderType::FileNamesContainer filenames;
  unsigned int numberOfFilenames =  inputFileNames.size();
  std::cout << "Input file names: " << numberOfFilenames << std::endl;
  std::cout << numberOfFilenames << std::endl;
  for(unsigned int fni = 0; fni<numberOfFilenames; fni++)
    {
    std::cout << "filename # " << fni << " = ";
    std::cout << inputFileNames[fni].c_str() << std::endl;
    filenames.push_back(inputFileNames[fni]);
    }

  reader->SetFileNames( filenames );

  reader->SetImageIO( dcmtkIO );

  reader->Update();

  writer->SetFileName( "dcmtk.nrrd" );
  writer->SetInput( reader->GetOutput() );

  writer->Update();

  reader->SetImageIO(gdcmIO);
  reader->Update();

  std::cout << "Orientation: " << reader->GetOutput()->GetDirection() << std::endl;

  writer->SetFileName( "gdcm.nrrd" );
  writer->SetInput( reader->GetOutput() );

  writer->Update();

  return EXIT_SUCCESS;
}
