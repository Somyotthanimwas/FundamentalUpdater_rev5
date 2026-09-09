#include "Config.h"

#include <fstream>
#include <iostream>


bool Config::Load(const std::string& filename)
{

    std::ifstream file(filename);


    if(!file)
    {
        std::cerr 
        << "Cannot open config file\n";

        return false;
    }


    std::string line;


    while(std::getline(file,line))
    {

        if(line.empty())
            continue;


        if(line[0]=='[')
            continue;


        auto pos=line.find('=');


        if(pos==std::string::npos)
            continue;


        std::string key =
            line.substr(0,pos);


        std::string value =
            line.substr(pos+1);


        values[key]=value;
    }


    return true;
}



std::string Config::Get(
    const std::string& key
)
{

    if(values.find(key)
       != values.end())
    {
        return values[key];
    }


    return "";

}
