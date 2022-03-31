#pragma once

#include <map>

class ConfigFileReader
{
public:
	ConfigFileReader(const char* filename);
	~ConfigFileReader();

    std::string getConfigName(const std::string& name);
    int setConfigValue(const std::string& name, const std::string& value);
private:
    void loadFile(const std::string& filename);
    int writeFile(const std::string& filename = "");
    void parseLine(char* line);
    char* trimSpace(char* name);

    bool loadOk_;
    std::map<std::string, std::string> configMap_;
    std::string configFile_;
};
