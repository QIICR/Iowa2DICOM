#include <iostream>
#include <set>
#include <vector>

// ITK includes
#include <itkImageFileReader.h>
#include <itkImageRegionConstIterator.h>

// CLP inclides
#include "CountSegmentationsCLP.h"

int main(int argc, char *argv[])
{
  PARSE_ARGS;

  typedef short PixelType;
  typedef itk::Image<PixelType,3> ImageType;
  typedef itk::ImageFileReader<ImageType> ReaderType;
  
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(inputSegmentationsFileName.c_str());
  reader->Update();
  ImageType::Pointer labelImage = reader->GetOutput();

  typedef itk::ImageRegionConstIterator<ImageType> IteratorType;
  IteratorType it(labelImage, labelImage->GetLargestPossibleRegion());
  it.GoToBegin();
  std::set<int> regionLabels;
  while(!it.IsAtEnd())
  {
    int labelValue = it.Get();
    if(labelValue > 0)
    {
      if(regionLabels.find(labelValue)==regionLabels.end())
      {
        regionLabels.insert(labelValue);
      }
    }
    ++it;
  }
  
  //std::vector<int> outputSegmentationCount;
  for (std::set<int>::iterator sit=regionLabels.begin(); sit!=regionLabels.end(); ++sit)
  {
    std::cout << *sit << " ";
    outputSegmentationCount.push_back(*sit);
  }

  std::cout << std::endl;
  return 0;
}
