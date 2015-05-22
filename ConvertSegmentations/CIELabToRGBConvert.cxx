#include "CIELabToRGBConvertCLP.h"

#include "../Common/SegmentAttributes.h"

#include <stdlib.h>
#include <iostream>
#include <vector>

int main( int argc, char* argv[] )
{
  PARSE_ARGS;


  if(cielabStr != ""){
    std::vector<std::string> inputTokens;
    unsigned cielabScaled[3];
    TokenizeString(cielabStr,inputTokens,",");
    std::cout << "Input CIELabScaled: ";
    for(int i=0;i<3;i++){
      cielabScaled[i] = atoi(inputTokens[i].c_str());
      std::cout << cielabScaled[i] << " ";
    }
    std::cout << std::endl;

    float cielab[3], ciexyz[3];
    unsigned rgb[3];
    getCIELabFromIntegerScaledCIELab(&cielabScaled[0],&cielab[0]);
    std::cout << "CIELab: ";
    for(int i=0;i<3;i++)
      std::cout << cielab[i] << " ";
    std::cout << std::endl;    

    getCIEXYZFromCIELab(&cielab[0],&ciexyz[0]);
    std::cout << "CIEXYZ: ";
    for(int i=0;i<3;i++)
      std::cout << ciexyz[i] << " ";
    std::cout << std::endl;    

    getRGBFromCIEXYZ(&ciexyz[0],&rgb[0]);
    std::cout << "RGB: ";
    for(int i=0;i<3;i++)
      std::cout << rgb[i] << " ";
    std::cout << std::endl;    
  }

  if(rgbStr != ""){
    std::vector<std::string> inputTokens;
    std::cout << rgbStr << std::endl;
    unsigned cielabScaled[3];
    float cielab[3], ciexyz[3];
    unsigned rgb[3];
    TokenizeString(rgbStr,inputTokens,",");
    std::cout << "Input RGB: ";
    for(int i=0;i<3;i++){
      rgb[i] = atoi(inputTokens[i].c_str());
      std::cout << rgb[i] << " ";
    }  
    std::cout << std::endl;

    getCIEXYZFromRGB(&rgb[0],&ciexyz[0]);
    std::cout << "CIEXYZ: ";
    for(int i=0;i<3;i++)
      std::cout << ciexyz[i] << " ";
    std::cout << std::endl;    

    getCIELabFromCIEXYZ(&ciexyz[0],&cielab[0]);
    std::cout << "CIELab: ";
    for(int i=0;i<3;i++)
      std::cout << cielab[i] << " ";
    std::cout << std::endl;    

    getIntegerScaledCIELabFromCIELab(&cielab[0],&cielabScaled[0]);
    std::cout << "CIELabScaled: ";
    for(int i=0;i<3;i++)
      std::cout << cielabScaled[i] << " ";
    std::cout << std::endl;    

  }

  return EXIT_SUCCESS;
}
