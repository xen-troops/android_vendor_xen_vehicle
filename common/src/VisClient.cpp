/*
 * Copyright (C) 2019 EPAM Systems Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

// #define DUMP_ALL_PARSED_RESULTS
// #define LOG_NDEBUG 0
#define LOG_TAG "visclient"

#include <cutils/properties.h>
#include <log/log.h>

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <utility>

#include "VisClient.h"

namespace epam {

VisClient::VisClient() : mConnectedState(ConnState::STATE_DISCONNECTED) {
    pereodicalGetEnabled = false;
    mWs = nullptr;
    recoverSubscribe = false;
    mServerConnectionHandler = nullptr;
}

void VisClient::setUri(const std::string& uri) {
    std::lock_guard<std::mutex> lock(mLock);
    mUri = uri;
}

void VisClient::start() {
    ALOGV("start");
    if (mIsActive) {
        ALOGW("Vis has started. Restarting...");
        stop();
    }
    mIsActive = true;
    mThread = std::thread([this]() {
        init();
        while (mIsActive == true) {
            poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
        }
        if (mWs) {
            mWs->close();
            mWs = nullptr;
        }
        {
            std::lock_guard<std::mutex> lock(mLock);
            mConnectedState = ConnState::STATE_DISCONNECTED;
        }
    });
    ALOGV("VisClient thread has started ...");
}

bool VisClient::waitConnectionS(int s) const {
    ALOGV("waitConnectionMs");
    int minSleepTime = std::min(s, 1);
    int timeSleeped = 0;
    std::unique_lock<std::mutex> lock(mLock);
    while (mConnectedState != epam::ConnState::STATE_CONNECTED) {
        lock.unlock();
        sleep(minSleepTime);
        lock.lock();
        timeSleeped += minSleepTime;
        if (timeSleeped >= s) break;
    }
    return mConnectedState == epam::ConnState::STATE_CONNECTED;
}

void VisClient::stop() {
    ALOGV("stop");
    mIsActive = false;
    if (mThread.joinable()) {
        mThread.join();
    }
    ALOGI("VisClient thread has stopped ...");
}

int VisClient::poll() {
    mLock.lock();
    ALOGV("poll, connection state is %d", static_cast<int>(mConnectedState));
    if (mConnectedState == ConnState::STATE_DISCONNECTED) {
        doInit();
    } else if (mConnectedState == ConnState::STATE_CONNECTED) {
#ifdef RECOVER_SUBSCRIBE
        for (auto it = mRecoverObservers.begin(); it != mRecoverObservers.end();) {
            ALOGI("Found some observer %du for recovery", it->first);
            int id = sendWSMessage(requestTemplateSubscribe, it->second.propertyName.c_str());
            if (id < 0) {
                ALOGE("Unable to recover observer, will do it later %du", it->first);
                ++it;
                continue;
            }

            SubscriptionRequest sr{it->second.handler, it->second.propertyName,
                                   static_cast<size_t>(id)};
            mSubscriptionObservers.insert(std::make_pair(id, sr));
            std::promise<WMessageResult> p;
            mPromised.emplace(std::pair<int, std::promise<WMessageResult> >(id, std::move(p)));
            it = mRecoverObservers.erase(it);
        }
#endif
    }
    mLock.unlock();

    // poll can trigger other event under mLock
    mHub.poll();
    return 0;
}

int VisClient::doInit() {
    ALOGV("doInit");
    mRequestId = 0;
    mProtocolErrorCount = 0;
    ALOGD("Will try to connect to VIS uri[%s]", mUri.c_str());
    mHub.connect(mUri.c_str(), reinterpret_cast<void*>(CONNECTION_MODE_SSL));
    mConnectedState = ConnState::STATE_CONNECTING;
    return 0;
}

int VisClient::init() {
    std::lock_guard<std::mutex> lock(mLock);
    ALOGV("init");

    if (mUri.empty()) {
        char propValue[PROPERTY_VALUE_MAX] = {};
        property_get("persist.vis.uri", propValue, "wss://192.168.0.1:8088");
        mUri = propValue;
    }

    std::function<void(void*)> errorHandler =
        std::bind(&VisClient::handleError, this, std::placeholders::_1);

    mHub.onError(errorHandler);

    std::function<void(uWS::WebSocket<uWS::CLIENT>*)> transferHandler =
        std::bind(&VisClient::handleTransfer, this, std::placeholders::_1);

    mHub.onTransfer(transferHandler);

    std::function<void(uWS::WebSocket<uWS::CLIENT>*, char*, size_t, uWS::OpCode)> messageHandler =
        std::bind(&VisClient::handleMessage, this, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3, std::placeholders::_4);

    mHub.onMessage(messageHandler);

    std::function<void(uWS::WebSocket<uWS::CLIENT>*, uWS::HttpRequest)> connectionHandler =
        std::bind(&VisClient::handleConnection, this, std::placeholders::_1, std::placeholders::_2);

    mHub.onConnection(connectionHandler);

    std::function<void(uWS::WebSocket<uWS::CLIENT>*, int, char*, size_t)> disconnectionHandler =
        std::bind(&VisClient::handleDisconnection, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
    mHub.onDisconnection(disconnectionHandler);

    return doInit();
}

/* Working with VIS data */

Status VisClient::getProperty(const std::string& propertyPath,
                              std::future<WMessageResult>& future) {
    std::lock_guard<std::mutex> lock(mLock);
    if (mConnectedState == ConnState::STATE_DISCONNECTED) {
        return Status::INVALID_STATE;
    }
    int id = sendWSMessage(requestTemplateGet, propertyPath.c_str());

    if (id < 0) {
        return Status::UNKNOWN_ERROR;
    }

    std::promise<WMessageResult> p;
    future = p.get_future();
    mPromised.emplace(std::pair<int, std::promise<WMessageResult> >(id, std::move(p)));
    return Status::OK;
}

Status VisClient::setProperty(const std::string& propertySet, std::future<WMessageResult>& future) {
    std::lock_guard<std::mutex> lock(mLock);
    if (mConnectedState == ConnState::STATE_DISCONNECTED) {
        return Status::INVALID_STATE;
    }
    int id = sendWSMessage(requestTemplateSet, propertySet.c_str());

    if (id < 0) {
        return Status::UNKNOWN_ERROR;
    }

    std::promise<WMessageResult> p;
    future = p.get_future();
    mPromised.emplace(std::pair<int, std::promise<WMessageResult> >(id, std::move(p)));
    return Status::OK;
}

Status VisClient::getPropertySync(const std::string& propertyGet, WMessageResult& result) {
    std::future<WMessageResult> f;
    Status st = getProperty(propertyGet, f);
    if (st != Status::OK) return st;
    if (f.wait_for(std::chrono::seconds(kMaxWsMessageDelaySec)) != std::future_status::ready) {
        ALOGE("setPropertySync timeout %d !", kMaxWsMessageDelaySec);
        return Status::UNKNOWN_ERROR;
    }
    result = f.get();
    return result.status;
}

Status VisClient::setPropertySync(const std::string& propertySet) {
    std::future<WMessageResult> f;
    Status st = setProperty(propertySet, f);
    if (st != Status::OK) return st;
    if (f.wait_for(std::chrono::seconds(kMaxWsMessageDelaySec)) != std::future_status::ready) {
        ALOGE("setPropertySync timeout for %d second(s) !", kMaxWsMessageDelaySec);
        return Status::UNKNOWN_ERROR;
    }
    WMessageResult sr = f.get();
    return sr.status;
}

void VisClient::setPereodicalGet(bool enable) {
    std::lock_guard<std::mutex> lock(mLock);
    pereodicalGetEnabled = enable;
}

int VisClient::sendWSMessage(const char* templ, const char* param) {
    if (mWs) {
        char buffer[kMaxBufferLength];
        snprintf(buffer, kMaxBufferLength, templ, param, ++mRequestId);
        mWs->send(buffer);
        return mRequestId;
    } else {
        ALOGE("Unable to send command through, no websocket.");
    }
    return -1;
}

int VisClient::sendWSMessage(const char* templ, const char* param, std::string& message) {
    if (mWs) {
        char buffer[kMaxBufferLength];
        snprintf(buffer, kMaxBufferLength, templ, param, ++mRequestId);
        mWs->send(buffer);
        message.assign(buffer);
        return mRequestId;
    } else {
        ALOGE("Unable to send command through, no websocket.");
    }
    return -1;
}

Status VisClient::doSubscribeProperty(const std::string& propertyName, CommandHandler observer,
                                      std::future<WMessageResult>& future) {
    if (mConnectedState == ConnState::STATE_DISCONNECTED) {
        return Status::INVALID_STATE;
    }

    std::string message;
    int id = sendWSMessage(requestTemplateSubscribe, propertyName.c_str(), message);

    if (id < 0) {
        return Status::UNKNOWN_ERROR;
    }

    SubscriptionRequest sr{observer, propertyName, static_cast<size_t>(id)};

    mSubscriptionObservers.insert(std::make_pair(id, sr));
    std::promise<WMessageResult> p;
    future = p.get_future();
    mPromised.emplace(std::pair<int, std::promise<WMessageResult> >(id, std::move(p)));
    return Status::OK;
}

Status VisClient::subscribeProperty(const std::string& propertyName, CommandHandler observer,
                                    std::future<WMessageResult>& future) {
    std::lock_guard<std::mutex> lock(mLock);
    return doSubscribeProperty(propertyName, observer, future);
}

Status VisClient::unsubscribe(const std::string& subscriptionId, CommandHandler observer,
                              std::future<WMessageResult>& sfuture) {
    std::lock_guard<std::mutex> lock(mLock);
    if (mConnectedState == ConnState::STATE_DISCONNECTED) {
        return Status::INVALID_STATE;
    }

    int id = sendWSMessage(requestTemplateUnsubscribe, subscriptionId.c_str());

    if (id < 0) {
        return Status::UNKNOWN_ERROR;
    }
    SubscriptionRequest sr{observer, subscriptionId, static_cast<size_t>(id)};
    mSubscriptionObservers.insert(std::make_pair(id, sr));
    std::promise<WMessageResult> p;
    sfuture = p.get_future();
    mPromised.emplace(std::pair<int, std::promise<WMessageResult> >(id, std::move(p)));
    return Status::OK;
}

Status VisClient::unsubscribeAll(std::future<WMessageResult>& future) {
    std::lock_guard<std::mutex> lock(mLock);
    if (mConnectedState == ConnState::STATE_DISCONNECTED) {
        return Status::INVALID_STATE;
    }

    int id = sendWSMessage(requestTemplateUnsubscribeAll, "unsubscribeAll");

    if (id < 0) {
        return Status::UNKNOWN_ERROR;
    }
    std::promise<WMessageResult> p;
    future = p.get_future();
    mPromised.emplace(std::pair<int, std::promise<WMessageResult> >(id, std::move(p)));
    return Status::OK;
}

/* uWS specific handlers*/

void VisClient::handleServerDisconnect() {
    mConnectedState = ConnState::STATE_DISCONNECTED;
    mWs = nullptr;
}

void VisClient::handleError(void* user) {
    ALOGV("handleError");
    std::lock_guard<std::mutex> lock(mLock);

    switch (reinterpret_cast<size_t>(user)) {
        case CONNECTION_MODE_NON_SSL:
            ALOGE("Client emitted error in non SSL MODE...");
            usleep(kSleepTimeAfterErrorUs);
            break;
        case CONNECTION_MODE_SSL:
            ALOGE("Client emitted error in SSL MODE...");
            usleep(kSleepTimeAfterErrorUs);
            break;
        default:
            ALOGE("FAILURE: %zd should not emit error!", reinterpret_cast<size_t>(user));
    }
    handleServerDisconnect();
}

void VisClient::handleTransfer(uWS::WebSocket<uWS::CLIENT>* ws) {
    std::lock_guard<std::mutex> lock(mLock);
    ALOGV("handleTransfer");
    ws->close();
    mWs = nullptr;
}

void VisClient::handleMessage(uWS::WebSocket<uWS::CLIENT>* ws, char* c, size_t t, uWS::OpCode op) {
    std::lock_guard<std::mutex> lock(mLock);
    ALOGV("handleMessage");
    if (op == uWS::OpCode::TEXT) {
        std::string str(c, t);
        Json::Value rootVal;
        Json::Reader reader;
        if (reader.parse(str, rootVal)) {
            std::map<std::string, Json::Value> result;
            parseJsonToStrings(rootVal, kAllTag, result);
#ifdef DUMP_ALL_PARSED_RESULTS
            for (auto it : result) {
                ALOGV("%s|%s", it.first.c_str(), it.second.toStyledString().c_str());
            }
#endif
            handleVisResponce(result);
        }
        return;
    }
    ws->close();
    mWs = nullptr;
}

void VisClient::handleConnection(uWS::WebSocket<uWS::CLIENT>* ws, uWS::HttpRequest /*req*/) {
    ALOGV("handleConnection");
    std::unique_lock<std::mutex> lock(mLock);
    switch (reinterpret_cast<size_t>(ws->getUserData())) {
        case CONNECTION_MODE_NON_SSL:
            ALOGE("Client established a remote connection over non-SSL");
            ws->close();
            mWs = nullptr;
            break;
        case CONNECTION_MODE_SSL:
            ALOGI("Client established a remote connection over SSL");
            mConnectedState = ConnState::STATE_CONNECTED;
            mWs = ws;
#ifdef RECOVER_SUBSCRIBE
            if ((mRecoverObservers.size() > 0) && (mObservers.size() > 0)) {
                ALOGE("Race between recovery and connect");
                abort();
            }

            for (auto& item : mObservers) {
                recoverSubscribe = true;
                mRecoverObservers.emplace(std::move(item));
            }
            mObservers.clear();
            mSubscriptionObservers.clear();
#endif
            lock.unlock();
            if (mServerConnectionHandler) mServerConnectionHandler(true);
            lock.lock();
            break;
        default:
            ALOGE("FAILURE: %zd should not connect!", reinterpret_cast<size_t>(ws->getUserData()));
            ws->close();
            mWs = nullptr;
    }
}

void VisClient::handleDisconnection(uWS::WebSocket<uWS::CLIENT>* ws, int code, char* message,
                                    size_t length) {
    {
        std::unique_lock<std::mutex> lock(mLock);
        ALOGI("Client got disconnected with data: %zd code %d message:%s",
              reinterpret_cast<size_t>(ws->getUserData()), code,
              std::string(message, length).c_str());
        handleServerDisconnect();
    }
    if (mServerConnectionHandler) mServerConnectionHandler(false);
}

/* Parsing */

void VisClient::parseJsonToStrings(const Json::Value& rootValue, const std::string& rootKey,
                                   std::map<std::string, Json::Value>& result) {
    if (rootValue.isNull()) {
        result[rootKey] = rootValue;
        return;
    }
    if (rootValue.isObject()) {
        Json::Value::Members members(rootValue.getMemberNames());
        for (unsigned int i = 0; i < members.size(); i++) {
            std::string str = members[i];
            Json::Value val = rootValue[str];
            if ((val.type() == Json::objectValue) || (val.type() == Json::arrayValue)) {
                parseJsonToStrings(val, str, result);
            }
            result[str] = val;
        }
        return;
    } else if (rootValue.isArray()) {
        bool hasNonObject = false;  // Will save array with non Object values
        for (unsigned int i = 0; i < rootValue.size(); i++) {
            if (rootValue[i].isObject())
                parseJsonToStrings(rootValue[i], rootKey, result);
            else
                hasNonObject = true;
        }
        if (hasNonObject) result[rootKey] = rootValue;
        return;
    }
    ALOGE("Parsing failed %s", rootValue.toStyledString().c_str());
    abort();
}

void VisClient::handleVisResponceGet(const std::map<std::string, Json::Value>& result) {
    /* GET responce */
    /* "required": ["action", "requestId", "value", "timestamp"] */

    size_t requestId = 0;

    auto rid = result.find(kRequestIdTag);
    if (rid != result.end()) {
        requestId = std::stoi(rid->second.asCString());
    } else {
        ALOGE("Malformed GET response from VIS: no required field \"requestId\"");
        return;
    }

    auto error = result.find(kErrorTag);
    if (error != result.end()) {
        ALOGE("Error on request %zu , error: %s", requestId,
              error->second.toStyledString().c_str());
        auto p = mPromised.find(requestId);
        if (p == mPromised.end()) {
            ALOGE("Unable to send error, message requestId=%zu is not registered", requestId);
            return;
        }
        auto promise = std::move(mPromised[requestId]);
        WMessageResult sr{Status::UNKNOWN_ERROR, requestId};
        promise.set_value(sr);
        mPromised.erase(p);
        return;
    }

    auto value = result.find(kValueTag);
    if (value == result.end()) {
        ALOGE("Malformed GET response from VIS: no required field \"value\"");
        return;
    }

    auto timestamp = result.find(kTimestampTag);
    if (timestamp == result.end()) {
        ALOGE("Malformed GET response from VIS: no required field \"timestamp\"");
        return;
    }

    ALOGV("Received GET responce for request id =%zu", requestId);

    auto p = mPromised.find(requestId);
    if (p == mPromised.end()) {
        ALOGE("Message with requestId = %zu is not registered, unable to send result.", requestId);
        return;
    }

    auto promise = std::move(mPromised[requestId]);
    WMessageResult sr{Status::OK, requestId, result};
    promise.set_value(sr);
    mPromised.erase(p);
}

void VisClient::handleVisResponceSet(const std::map<std::string, Json::Value>& result) {
    /* SET responce */
    /* "required": ["action", "requestId", "timestamp"] */

    size_t requestId = 0;

    auto rid = result.find(kRequestIdTag);
    if (rid != result.end()) {
        requestId = std::stoi(rid->second.asCString());
    } else {
        ALOGE("Malformed SET response from VIS: no required field \"requestId\"");
        return;
    }

    auto error = result.find(kErrorTag);
    if (error != result.end()) {
        ALOGE("Error on request %zu , error: %s", requestId,
              error->second.toStyledString().c_str());
        auto p = mPromised.find(requestId);
        if (p == mPromised.end()) {
            ALOGE("Unable to send error, message requestId=%zu is not registered", requestId);
            return;
        }
        auto promise = std::move(mPromised[requestId]);
        WMessageResult sr{Status::UNKNOWN_ERROR, requestId};
        promise.set_value(sr);
        mPromised.erase(p);
        return;
    }

    auto timestamp = result.find(kTimestampTag);
    if (timestamp == result.end()) {
        ALOGE("Malformed SET response from VIS: no required field \"timestamp\"");
        return;
    }

    ALOGV("Received SET responce for request id =%zu", requestId);

    auto p = mPromised.find(requestId);
    if (p == mPromised.end()) {
        ALOGE("Message with requestId = %zu is not registered, unable to send result.", requestId);
        return;
    }

    auto promise = std::move(mPromised[requestId]);
    WMessageResult sr{Status::OK, requestId};
    promise.set_value(sr);
    mPromised.erase(p);
}

void VisClient::handleVisResponceSubscribe(const std::map<std::string, Json::Value>& result) {
    /* SUBSCRIBE responce */
    /* "required": ["action", "requestId", "subscriptionId", "timestamp"] */

    size_t requestId = 0, subscriptionId = 0;

    auto rid = result.find(kRequestIdTag);
    if (rid != result.end()) {
        requestId = std::stoi(rid->second.asCString());
    } else {
        ALOGE("Malformed SUBSCRIBE response from VIS: no required field \"requestId\"");
        return;
    }

    auto error = result.find(kErrorTag);
    if (error != result.end()) {
        ALOGE("Error on request %zu , error: %s", requestId,
              error->second.toStyledString().c_str());
        auto p = mPromised.find(requestId);
        if (p == mPromised.end()) {
            ALOGE("Unable to send error, message requestId=%zu is not registered", requestId);
            return;
        }
        auto promise = std::move(mPromised[requestId]);
        WMessageResult sr{Status::UNKNOWN_ERROR, requestId};
        promise.set_value(sr);
        mPromised.erase(p);
        return;
    }

    auto sid = result.find(kSubscriptionIdTag);
    if (sid != result.end()) {
        subscriptionId = std::stoi(sid->second.asCString());
    } else {
        ALOGE("Malformed subscribe message with no subscriptionId");
        return;
    }

    auto timestamp = result.find(kTimestampTag);
    if (timestamp == result.end()) {
        ALOGE("Malformed SUBSCRIBE response from VIS: no required field \"timestamp\"");
        return;
    }

    ALOGV("SUBSCRIBE response for request id =%zu  subscriptionId=%zu", requestId, subscriptionId);

    if (mSubscriptionObservers.find(requestId) == mSubscriptionObservers.end()) {
        ALOGE("Invalid subscriptionId = %zu", subscriptionId);
        auto p = mPromised.find(requestId);
        if (p == mPromised.end()) {
            ALOGE("Message with requestId = %zu not registered", requestId);
            return;
        }

        auto promise = std::move(mPromised[requestId]);
        WMessageResult sr{Status::UNKNOWN_ERROR, subscriptionId};
        promise.set_value(sr);
        ALOGV("Set value to promise !");
        // theoretically here we need to send unsubscribe command to VIS
        mPromised.erase(p);
        return;
    } else {
        auto p = mPromised.find(requestId);
        if (p != mPromised.end()) {
            auto promise = std::move(mPromised[requestId]);
            WMessageResult sr{Status::OK, subscriptionId};
            mObservers.insert(std::make_pair(subscriptionId,
                    std::move(mSubscriptionObservers[requestId])));
            mSubscriptionObservers.erase(requestId);
            promise.set_value(sr);
            mPromised.erase(p);
        }
    }
}

void VisClient::handleVisResponceUnsubscribe(const std::map<std::string, Json::Value>& result) {
    /* UNSUBSCRIBE */
    /* "required": ["action", "subscriptionId", "requestId", "timestamp"] */

    size_t requestId = 0, subscriptionId = 0;

    auto rid = result.find(kRequestIdTag);
    if (rid != result.end()) {
        requestId = std::stoi(rid->second.asCString());
    } else {
        ALOGE("Malformed UNSUBSCRIBE response from VIS: no required field \"requestId\"");
        return;
    }

    auto sid = result.find(kSubscriptionIdTag);
    if (sid != result.end()) {
        subscriptionId = std::stoi(sid->second.asCString());
    } else {
        ALOGE("Malformed UNSUBSCRIBE response from VIS: no required field \"subscriptionId\"");
        return;
    }

    auto timestamp = result.find(kTimestampTag);
    if (timestamp == result.end()) {
        ALOGE("Malformed UNSUBSCRIBE response from VIS: no required field \"timestamp\"");
        return;
    }

    auto error = result.find(kErrorTag);
    if (error != result.end()) {
        ALOGE("Error on request %zu , error: %s", requestId,
              error->second.toStyledString().c_str());
        auto p = mPromised.find(requestId);
        if (p == mPromised.end()) {
            ALOGE("Unable to send error, message requestId=%zu is not registered", requestId);
            return;
        }
        auto promise = std::move(mPromised[requestId]);
        WMessageResult sr{Status::UNKNOWN_ERROR, requestId};
        promise.set_value(sr);
        mPromised.erase(p);
        return;
    }

    ALOGV("Received subscription subscriptionId=%zu", subscriptionId);

    auto p = mPromised.find(requestId);
    if (p != mPromised.end()) {
        auto promise = std::move(mPromised[requestId]);
        WMessageResult sr{Status::OK, subscriptionId};
        mSubscriptionObservers.erase(requestId);
        mObservers.erase(subscriptionId);
        promise.set_value(sr);
        mPromised.erase(p);
    }
}

void VisClient::handleVisResponceUnsubscribeAll(const std::map<std::string, Json::Value>& result) {
    /* UNSUBSCRIBEALL */
    /* "required": ["action", "requestId", "error", "timestamp"] */

    size_t requestId = 0, subscriptionId = 0;

    auto rid = result.find(kRequestIdTag);
    if (rid != result.end()) {
        requestId = std::stoi(rid->second.asCString());
    } else {
        ALOGE("Malformed UNSUBSCRIBEALL response from VIS: no required field \"requestId\"");
        return;
    }

    auto timestamp = result.find(kTimestampTag);
    if (timestamp == result.end()) {
        ALOGE("Malformed UNSUBSCRIBEALL response from VIS: no required field \"timestamp\"");
        return;
    }

    auto error = result.find(kErrorTag);
    if (error != result.end()) {
        ALOGE("Error on request %zu , error: %s", requestId,
              error->second.toStyledString().c_str());
        auto p = mPromised.find(requestId);
        if (p == mPromised.end()) {
            ALOGE("Unable to send error, message requestId=%zu is not registered", requestId);
            return;
        }
        auto promise = std::move(mPromised[requestId]);
        WMessageResult sr{Status::UNKNOWN_ERROR, requestId};
        promise.set_value(sr);
        mPromised.erase(p);
        return;
    }

    ALOGV("Received subscription subscriptionId=%zu", subscriptionId);

    auto p = mPromised.find(requestId);
    if (p != mPromised.end()) {
        auto promise = std::move(mPromised[requestId]);
        WMessageResult sr{Status::OK, subscriptionId};
        mObservers.clear();
        promise.set_value(sr);
        mPromised.erase(p);
    }
}

void VisClient::handleVisResponceSubscription(const std::map<std::string, Json::Value>& result) {
    /* SUBSCRIPTION */
    /* "required": ["action", "subscriptionId", "value", "timestamp"] */

    size_t subscriptionId = 0;

    auto sid = result.find(kSubscriptionIdTag);
    if (sid != result.end()) {
        subscriptionId = std::stoi(sid->second.asCString());
    } else {
        ALOGE("Malformed SUBSCRIPTION notification from VIS: no subscriptionId");
        return;
    }

    auto value = result.find(kValueTag);
    if (value == result.end()) {
        ALOGE("Malformed SUBSCRIPTION notification from VIS: no required field \"value\"");
        return;
    }

    auto timestamp = result.find(kTimestampTag);
    if (timestamp == result.end()) {
        ALOGE("Malformed SUBSCRIPTION response from VIS: no required field \"timestamp\"");
        return;
    }

    ALOGV("Received subscription subscriptionId=%zu", subscriptionId);
    auto it = mObservers.find(subscriptionId);
    if (it != mObservers.end()) {
        it->second.handler(result);

    } else {
        ALOGE("Invalid subscription ID = %zu", subscriptionId);
        return;
    }
}

void VisClient::handleVisResponce(const std::map<std::string, Json::Value>& result) {
    const std::string action = result.at(kActionTag).asString();
    if (!strcmp(action.c_str(), kActionGetTag)) {
        handleVisResponceGet(result);
    } else if (!strcmp(action.c_str(), kActionSubscribeTag)) {
        handleVisResponceSubscribe(result);
    } else if (!strcmp(action.c_str(), kActionSubscriptionTag)) {
        handleVisResponceSubscription(result);
    } else if (!strcmp(action.c_str(), kActionUnsubscribeTag)) {
        handleVisResponceUnsubscribe(result);
    } else if (!strcmp(action.c_str(), kActionUnsubscribeAllTag)) {
        handleVisResponceUnsubscribeAll(result);
    } else if (!strcmp(action.c_str(), kActionSetTag)) {
        handleVisResponceSet(result);
    }
}

}  // namespace epam
