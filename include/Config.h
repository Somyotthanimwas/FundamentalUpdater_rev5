#include <string>
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>


class Config
{

private:

    std::map<std::string,std::string> values;


public:

    bool Load(const std::string& filename);


    std::string Get(
        const std::string& key
    );

};

#endif
