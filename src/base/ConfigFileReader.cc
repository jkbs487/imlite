/*
 * ConfigFileReader.cpp
 *
 *  Created on: 2013-7-2
 *      Author: ziteng@mogujie.com
 */
#include "ConfigFileReader.h"
#include "slite/Logger.h"

#include <cstring>
#include <cstdio>

using namespace slite;

ConfigFileReader::ConfigFileReader(const char* filename)
{
	loadFile(filename);
}

ConfigFileReader::~ConfigFileReader()
{
}

std::string ConfigFileReader::getConfigName(const std::string& name)
{
	if (!loadOk_)
		return nullptr;

	std::string value;
	std::map<std::string, std::string>::iterator it = configMap_.find(name);
	if (it != configMap_.end()) {
		value = it->second;
	}

	return value;
}

int ConfigFileReader::setConfigValue(const std::string& name, const std::string& value)
{
    if(!loadOk_)
        return -1;

    std::map<std::string, std::string>::iterator it = configMap_.find(name);
    if (it != configMap_.end()) {
        it->second = value;
    } else {
        configMap_.insert(std::make_pair(name, value));
    }
    return writeFile();
}

void ConfigFileReader::loadFile(const std::string& filename)
{
    configFile_.clear();
    configFile_.append(filename);
	FILE* fp = fopen(filename.c_str(), "r");
	if (!fp) {
		LOG_ERROR << "can not open " << filename << ": " << strerror(errno);
		return;
	}

	char buf[256];
	for (;;) {
		char* p = fgets(buf, 256, fp);
		if (!p)
			break;

		size_t len = strlen(buf);
		if (buf[len - 1] == '\n')
			buf[len - 1] = 0;			// remove \n at the end

		char* ch = strchr(buf, '#');	// remove string start with #
		if (ch)
			*ch = 0;

		if (strlen(buf) == 0)
			continue;

		parseLine(buf);
	}

	fclose(fp);
	loadOk_ = true;
}

int ConfigFileReader::writeFile(const std::string& filename)
{
   FILE* fp = nullptr;
   if (filename.empty()) {
       fp = fopen(configFile_.c_str(), "w");
   } else {
       fp = fopen(filename.c_str(), "w");
   }
   if (fp == nullptr) {
       return -1;
   }

   char szPaire[128];
   for (const auto& config : configMap_) {
		memset(szPaire, 0, sizeof(szPaire));
		snprintf(szPaire, sizeof(szPaire), "%s=%s\n", config.first.c_str(), config.second.c_str());
		size_t ret = fwrite(szPaire, strlen(szPaire), 1, fp);
		if (ret != 1) {
          fclose(fp);
          return -1;
      	}
	}
   fclose(fp);
   return 0;
}

void ConfigFileReader::parseLine(char* line)
{
	char* p = strchr(line, '=');
	if (p == nullptr)
		return;

	*p = 0;
	char* key = trimSpace(line);
	char* value = trimSpace(p + 1);
	if (key && value) {
		configMap_.insert(std::make_pair(key, value));
	}
}

char* ConfigFileReader::trimSpace(char* name)
{
	// remove starting space or tab
	char* startPos = name;
	while ( (*startPos == ' ') || (*startPos == '\t') )
	{
		startPos++;
	}

	if (strlen(startPos) == 0)
		return NULL;

	// remove ending space or tab
	char* endPos = name + strlen(name) - 1;
	while ( (*endPos == ' ') || (*endPos == '\t') )
	{
		*endPos = 0;
		endPos--;
	}

	int len = static_cast<int>(endPos - startPos) + 1;
	if (len <= 0)
		return nullptr;

	return startPos;
}
