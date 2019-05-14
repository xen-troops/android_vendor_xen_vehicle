
#define LOG_TAG "VIS_TEST"

#include <log/log.h>
#include <future>
#include <string>
#include <sstream>
#include <iostream>
#include <unistd.h>
#include "VisClient.h"

using namespace epam;

void resultHandler(const CommandResult& /*result*/) {
    ALOGI("Got subscribed result!");
}

void resultHandler2(const CommandResult& /*result*/) {
    ALOGI("Got subscribed result2!");
}


static const int MAX_F_TIME_SEC = 4;
void testSubscribeStop() {
    ALOGI("====================== testSubscribeStop =====================");
    VisClient mVis;
    mVis.start();
    int cnt = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    while (mVis.getConnectedState() != ConnState::STATE_CONNECTED) {
        sleep(1);
        if (++cnt > 10) {
            ALOGE("Unable to connect to VIS ");
            mVis.stop();
            return;
        }
    }
    ALOGI("Will try to subscribe ....");
    std::future<WMessageResult> f;
    std::string str("*");
    Status st = mVis.subscribeProperty( str,resultHandler,f);
    ALOGI("Subscribe retrned OK = %d", st == Status::OK);
    sleep(10);
    mVis.stop();

    ALOGI("======================*** testSubscribeStop ***=====================");
}

void testFailedSubscribe() {
    ALOGI("====================== testFailedSubscribe =====================");
    VisClient mVis;
    //mVis.start();
    ALOGI("Will try to subscribe ....");
    std::future<WMessageResult> f;
    std::string str("*");
    Status st = mVis.subscribeProperty( str,resultHandler,f);
    ALOGI("Subscribe retrned OK = %d", st == Status::OK);
    if (!f.valid()) {
        ALOGE("Returned future is not valid !");
        mVis.stop();
        return;
    }
    if (f.wait_for(std::chrono::seconds(MAX_F_TIME_SEC)) != std::future_status::ready) {
        ALOGE("Unable to receive result, timeout %d !!!!", MAX_F_TIME_SEC);
        mVis.stop();
        return;
    }
    WMessageResult sr = f.get();
    ALOGI("Subscription command retrned OK = %d", sr.status == Status::OK);
    mVis.stop();
    ALOGI("======================*** testFailedSubscribe ***=====================");
}

void testSubscribe() {
    ALOGI("====================== testSubscribe =====================");
    VisClient mVis;
    mVis.start();
    int cnt = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    while (mVis.getConnectedState() != ConnState::STATE_CONNECTED) {
        sleep(1);
        if (++cnt > 10) {
            ALOGE("Unable to connect to VIS ");
            mVis.stop();
            return;
        }
    }
    ALOGI("Will try to subscribe ....");
    std::future<WMessageResult> f;
    std::string str("*");
    Status st = mVis.subscribeProperty( str,resultHandler,f);
    ALOGI("Subscribe retrned OK = %d", st == Status::OK);
    if (!f.valid()) {
        ALOGE("Future is not valid !");
        mVis.stop();
        return;
    }
    if (f.wait_for(std::chrono::seconds(MAX_F_TIME_SEC)) != std::future_status::ready) {
        ALOGE("Unable to receive result, timeout %d !!!!", MAX_F_TIME_SEC);
        mVis.stop();
        return;
    }
    WMessageResult sr = f.get();
    ALOGI("Subscription command retrned OK = %d", sr.status == Status::OK);
    sleep(5);

    std::stringstream ss;
    ss << sr.subscriptionId;
    str = ss.str();
    st = mVis.unsubscribe(str, resultHandler, f);
    ALOGI("UnSubscribe retrned OK = %d", st == Status::OK);
    if (!f.valid()) {
        ALOGE("Future is not valid !");
        mVis.stop();
        return;
    }
    if (f.wait_for(std::chrono::seconds(MAX_F_TIME_SEC)) != std::future_status::ready) {
        ALOGE("Unable to receive result, timeout %d !!!!", MAX_F_TIME_SEC);
        mVis.stop();
        return;
    }
    sr = f.get();
    ALOGI("UnSubscription command retrned OK = %d", sr.status == Status::OK);
    //sleep(10);
    mVis.stop();

    ALOGI("======================*** testSubscribe ***=====================");
}

void testUnSubscribeAll() {
    ALOGI("====================== testUnSubscribeAll =====================");
    VisClient mVis;
    mVis.start();
    int cnt = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    while (mVis.getConnectedState() != ConnState::STATE_CONNECTED) {
        sleep(1);
        if (++cnt > 10) {
            ALOGE("Unable to connect to VIS ");
            mVis.stop();
            return;
        }
    }
    ALOGI("Will try to subscribe ....");
    std::future<WMessageResult> f;
    std::string str("*");
    Status st = mVis.subscribeProperty( str,resultHandler,f);
    ALOGI("Subscribe retrned OK = %d", st == Status::OK);
    if (!f.valid()) {
        ALOGE("Future is not valid !");
        mVis.stop();
        return;
    }
    if (f.wait_for(std::chrono::seconds(MAX_F_TIME_SEC)) != std::future_status::ready) {
        ALOGE("Unable to receive result, timeout %d !!!!", MAX_F_TIME_SEC);
        mVis.stop();
        return;
    }
    WMessageResult sr = f.get();
    ALOGI("Subscription command retrned OK = %d", sr.status == Status::OK);
    sleep(5);
    st = mVis.subscribeProperty( str,resultHandler2,f);
    ALOGI("Subscribe2 retrned OK = %d", st == Status::OK);
    if (!f.valid()) {
        ALOGE("Future is not valid !");
        mVis.stop();
        return;
    }
    if (f.wait_for(std::chrono::seconds(MAX_F_TIME_SEC)) != std::future_status::ready) {
        ALOGE("Unable to receive result, timeout %d !!!!", MAX_F_TIME_SEC);
        mVis.stop();
                return;
            }
    sr = f.get();
    ALOGI("Subscription2 command retrned OK = %d", sr.status == Status::OK);

    sleep(5);
    std::stringstream ss;
    ss << sr.subscriptionId;
    str = ss.str();
    st = mVis.unsubscribeAll(f);
    ALOGI("UnSubscribeAll retrned OK = %d", st == Status::OK);
    if (!f.valid()) {
        ALOGE("Future is not valid !");
        mVis.stop();
        return;
    }
    if (f.wait_for(std::chrono::seconds(MAX_F_TIME_SEC)) != std::future_status::ready) {
        ALOGE("Unable to receive result, timeout %d !!!!", MAX_F_TIME_SEC);
        mVis.stop();
        return;
    }
    sr = f.get();
    ALOGI("UnSubscribeAll command retrned OK = %d", sr.status == Status::OK);
    //sleep(10);
    mVis.stop();

    ALOGI("======================*** testSubscribe ***=====================");
}


void testSet() {
    ALOGI("====================== testSet =====================");
    VisClient mVis;
    mVis.start();
    while (mVis.getConnectedState() != ConnState::STATE_CONNECTED) {
        sleep(1);
    }
    std::future<WMessageResult> f;
    std::string str("\"XXXX\":10");
    ALOGI("Will try to set %s ....", str.c_str());
    Status st = mVis.setProperty(str, f);
    ALOGI("SET retrned OK = %d", st == Status::OK);
    f.wait();
    WMessageResult sr = f.get();
    ALOGI("SET command retrned OK = %d", sr.status == Status::OK);
    sleep(5);
    //sleep(10);
    mVis.stop();

    ALOGI("======================*** testSet ***=====================");
}

void testSetSync() {
    ALOGI("====================== testSetSync =====================");
    VisClient mVis;
    mVis.start();
    while (mVis.getConnectedState() != ConnState::STATE_CONNECTED) {
        sleep(1);
    }
    std::string str("\"Test.Vehicle.hvac.fan_speed\":12");
    Status st = mVis.setPropertySync(str);
    ALOGI("SET retrned OK = %d", st == Status::OK);
    sleep(3);
    mVis.stop();

    ALOGI("======================*** testSetSync ***=====================");
}

void testGetSync() {
    ALOGI("====================== testGetSync =====================");
    VisClient mVis;
    mVis.start();
    while (mVis.getConnectedState() != ConnState::STATE_CONNECTED) {
        sleep(1);
    }
    std::string str("Test.Vehicle.hvac.fan_speed");
    WMessageResult sr;
    Status st = mVis.getPropertySync(str, sr);
    ALOGI("GET retrned OK = %d", st == Status::OK);

    std::string result("value");

    auto it = sr.commandResult.find(result);
    if (it != sr.commandResult.end()) {
        ALOGI("%s = %s", str.c_str(),it->second.toStyledString().c_str());
    }
    sleep(3);
    mVis.stop();

    ALOGI("======================*** testGetSync ***=====================");
}


void testSetGetSync() {
    ALOGI("====================== testSetGetSync =====================");
    VisClient mVis;
    mVis.start();
    while (mVis.getConnectedState() != ConnState::STATE_CONNECTED) {
        sleep(1);
    }

    int funSpeed = 21;
    std::string sFunSpeed("Test.Vehicle.hvac.fan_speed");
    for (int i = 0; i < 10; i++) {

    {
        std::stringstream ss;
        ss << "\"" << sFunSpeed << "\":" << funSpeed;
        Status st = mVis.setPropertySync(ss.str());
        if (st != Status::OK) {
            ALOGE("Set fail");
            mVis.stop();
            return;
        }
        ALOGI("SET retrned OK = %d", st == Status::OK);

    }
    WMessageResult sr;
    Status st = mVis.getPropertySync(sFunSpeed, sr);
    if (st != Status::OK) {
        ALOGE("Get fail");
        mVis.stop();
        return;
    }
    ALOGI("GET retrned OK = %d", st == Status::OK);
    std::string value("value");
    auto it = sr.commandResult.find(value);
    if (it != sr.commandResult.end()) {
        ALOGI("%s = %s  eq=%d", sFunSpeed.c_str(),it->second.toStyledString().c_str(), std::stoi(it->second.toStyledString()) == funSpeed);
    }
    funSpeed++;
    }
    sleep(3);
    mVis.stop();

    ALOGI("======================*** testSetGetSync ***=====================");
}

void testAsyncSubscribe() {
    int num = 10;
    std::future<void> f[num];
    for (int i = 0; i < num ; i++){
         f[i] =  std::async(std::launch::async, testSubscribe);
    }
}

int main () {
    std::cout << "Test started \n";
    testFailedSubscribe();
    std::cout << "Test testFailedSubscribe completed \n";
    testSubscribeStop();
    std::cout << "Test testSubscribeStop completed \n";
    testSetSync();
    std::cout << "Test testSetSync completed \n";
    testGetSync();
    std::cout << "Test testGetSync completed \n";
    testSetGetSync();
    std::cout << "Test testSetGetSync completed \n";
    testSubscribe();
    std::cout << "Test testSubscribe completed \n";
    testAsyncSubscribe();
    std::cout << "Test testAsyncSubscribe completed \n";
    testUnSubscribeAll();
    std::cout << "Test testUnSubscribeAll completed \n";
    return 0;
}



