#ifndef SegmentAttributes_h
#define SegmentAttributes_h

#include <string>
#include <vector>
#include <map>

#include "dcmtk/dcmiod/iodmacro.h"

/*
 *  * Tokenize input string with space as a delimiter
 *   */
void TokenizeString(std::string str, std::vector<std::string> &tokens, std::string delimiter){
    // http://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
    size_t pos = 0;
    while((pos = str.find(delimiter)) != std::string::npos) {
      std::string token = str.substr(0,pos);
      tokens.push_back(token);
      str.erase(0, pos+delimiter.length());
    }
    tokens.push_back(str);
};

void SplitString(std::string str, std::string &head, std::string &tail, std::string delimiter){
    // http://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
    size_t pos = str.find(delimiter);
    if(pos != std::string::npos) {
      head = str.substr(0,pos);
      tail = str.substr(pos+delimiter.length(),str.length()-1);
    }
};

CodeSequenceMacro StringToCodeSequenceMacro(std::string str){
  std::string tail, code, designator, meaning;
  SplitString(str, code, tail, ",");
  SplitString(tail, designator, meaning, ",");
  return CodeSequenceMacro(code.c_str(), designator.c_str(), meaning.c_str());
}

class SegmentAttributes {
  public:
    SegmentAttributes(){};

    SegmentAttributes(unsigned labelID){
      this->labelID = labelID;
    }
    ~SegmentAttributes(){};

    void setLabelID(unsigned labelID){
      this->labelID = labelID;
    }

    std::string lookupAttribute(std::string key){
      if(attributesDictionary.find(key) == attributesDictionary.end())
        return "";
      return attributesDictionary[key];
    }

    int populateAttributesFromString(std::string attributesStr){
      std::vector<std::string> tupleList;
      TokenizeString(attributesStr,tupleList,";");
      for(std::vector<std::string>::const_iterator t=tupleList.begin();t!=tupleList.end();++t){
        std::vector<std::string> tuple;
        TokenizeString(*t,tuple,":");
        if(tuple.size()==2)
          attributesDictionary[tuple[0]] = tuple[1];
      }
      return 0;
    }

    void PrintSelf(){
      std::cout << "LabelID: " << labelID << std::endl;
      for(std::map<std::string,std::string>::const_iterator mIt=attributesDictionary.begin();
          mIt!=attributesDictionary.end();++mIt){
        std::cout << (*mIt).first << " : " << (*mIt).second << std::endl;
      }
      std::cout << std::endl;
    }

  private:
    unsigned labelID;
    std::map<std::string, std::string> attributesDictionary;
};

#endif
