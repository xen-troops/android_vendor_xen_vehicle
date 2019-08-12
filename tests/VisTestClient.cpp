#define LOG_TAG "VIS_TEST"

#include <log/log.h>
#include <future>
#include <string>
#include <sstream>
#include <iostream>
#include <unistd.h>

#include <fstream>
#include <map>

#undef TRY_AGAIN

#include <json/reader.h>

#include "VisClient.h"

using namespace epam;

VisClient mVis;

const std::string usage = "Please use as following:\n"\
                    "vis_client_test _SENSOR_DATA_ [_VIS_URI_]";
const static char* ksKey = "key";
const static char* ksValue = "value";
const static char* ksTs = "ts";

struct Record {
    std::string key;
    std::string value;
};

std::map<std::string, Record> gEvents;

int main (int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << usage << std::endl;
        return -1;
    }
    std::ifstream configFile(argv[1]);
    if (!configFile.is_open()) {
        std::cout << "Unable to open " << argv[1] << std::endl; 
        return -2;
    } else {
        Json::Value root;
        Json::Reader reader;
        if (reader.parse(configFile, root, false)) {
            if (root.isArray()) {
                for (unsigned int i = 0; i < root.size(); i++) {
                    Json::Value val = root[i];
                    const std::string& key = val[ksKey].asString();
                    const std::string& value = val[ksValue].asString();
                    const std::string& ts = val[ksTs].asString();
                    ALOGD("Parced key=%s value=%s ts=%s", key.c_str(), value.c_str(), ts.c_str());
                    gEvents.emplace(ts, Record{key,value});
                }
            }
        } else {
            std::cout << "Unable to parse " << argv[1] << std::endl;
            return -2;
        }
    }
    ALOGI("====== Starting Vis Test Client =========");
    if (argc > 2 )
        mVis.setUri(argv[2]);
    else 
        mVis.setUri("wss://wwwivi:8088");
    mVis.start();
    if (!mVis.waitConnectionS(10)) {
        ALOGE("Unable to connect to VIS SERVER");
         mVis.stop();
        return -3;
    }

    //Status st = mVis.setPropertySync(std::string{"Test.Vehicle.hvac.fan_speed"}, std::string{"12"});
    //ALOGI("SET retrned OK = %d", st == Status::OK);

    while (1) {
        int previous = 0;
        for (auto val: gEvents) {
            Status st = mVis.setPropertySync(val.second.key, val.second.value);
            ALOGI("SET retrned OK = %d", st == Status::OK);
            sleep(1);
            previous = atoi(val.first.c_str());
        }
    }
    sleep(3);
    mVis.stop();
    ALOGI("====== End Vis Test Client =========");
    return 0;
}
