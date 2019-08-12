/*
 * Copyright (C) 2018 EPAM Systems Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef COMMON_INCLUDE_VHAL_V2_0_VISCLIENT_H_
#define COMMON_INCLUDE_VHAL_V2_0_VISCLIENT_H_

#include "uWS.h"
#include <atomic>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#undef TRY_AGAIN

#include <json/reader.h>

#define RECOVER_SUBSCRIBE

namespace epam {

/*******************************************************************************
 * Enum class which identifies possible connection states of the visclient.
 * @ingroup visclient
 ******************************************************************************/
enum class ConnState { STATE_DISCONNECTED, STATE_CONNECTING, STATE_CONNECTED };

/*******************************************************************************
 * Enum class used for error handling in visclient.
 * @ingroup visclient
 ******************************************************************************/
enum class Status { OK, INVALID_STATE, UNKNOWN_ERROR };

/*******************************************************************************
 * Different connection modes.
 * @ingroup visclient
 ******************************************************************************/
enum ConnectionMode { CONNECTION_MODE_NON_SSL, CONNECTION_MODE_SSL };

typedef std::map<std::string, Json::Value> CommandResult;
typedef std::function<void(const CommandResult& result)> CommandHandler;
typedef std::function<void(bool)> ConnectionHandler;

struct WMessageResult {
    WMessageResult() = default;
    WMessageResult(Status s, size_t id) : status(s), subscriptionId(id) {}
    WMessageResult(Status s, size_t id, CommandResult c)
        : status(s), subscriptionId(id), commandResult(c) {}
    Status status;
    size_t subscriptionId;
    CommandResult commandResult;
};

struct SubscriptionRequest {
    CommandHandler handler;
    std::string propertyName;
    size_t requestId;
};

/*******************************************************************************
 * VIS client implementation
 * https://www.w3.org/TR/vehicle-information-service
 * @ingroup visclient
 ******************************************************************************/
class VisClient {
 public:
    VisClient();
    ~VisClient() = default;

    /**
     * @param uri     VIS server web socket URI. Only secure connections supported(wss).
     */
    void setUri(const std::string& uri);

    /* Asynchronous API */
    /* Caller must check return result(and not use future if result is not OK)
     * or validity of the future */

    Status getProperty(const std::string& propertyGet, std::future<WMessageResult>& future);
    Status setProperty(const std::string& propertyPath, const std::string& propertyValue, std::future<WMessageResult>& future);
    Status subscribeProperty(const std::string& propertyName, CommandHandler observer,
                             std::future<WMessageResult>& future);
    Status unsubscribe(const std::string& subscriptionId, CommandHandler observer,
                       std::future<WMessageResult>& future);
    Status unsubscribeAll(std::future<WMessageResult>& future);

    /* Synchronous API */

    Status getPropertySync(const std::string& propertyGet, WMessageResult& result);
    Status setPropertySync(const std::string& propertyPath, const std::string& propertyValue);
    Status subscribePropertySync(const std::string& propertyName, CommandHandler observer,
                                 WMessageResult& result);
    Status unsubscribeSync(const std::string& subscriptionId, CommandHandler observer,
                           WMessageResult& result);
    Status unsubscribeAllSync();

    /* Connection API */
    void start();
    void stop();
    ConnState getConnectedState() const { return mConnectedState; }
    bool waitConnectionS(int s) const;

    void registerServerConnectionhandler(const ConnectionHandler& ch) {
        mServerConnectionHandler = ch;
    }

 private:

    int init();
    int doInit();
    int poll();

    int sendWSMessage(const char* message);
    int sendWSMessage(const char* templ, const char* param);
    int sendWSMessage(const char* templ, const char* param, std::string& message);
    void handleVisResponce(const std::map<std::string, Json::Value>& result);
    void handleVisResponceGet(const std::map<std::string, Json::Value>& result);
    void handleVisResponceSet(const std::map<std::string, Json::Value>& result);
    void handleVisResponceSubscribe(const std::map<std::string, Json::Value>& result);
    void handleVisResponceUnsubscribe(const std::map<std::string, Json::Value>& result);
    void handleVisResponceUnsubscribeAll(const std::map<std::string, Json::Value>& result);
    void handleVisResponceSubscription(const std::map<std::string, Json::Value>& result);

    void handleServerDisconnect();

    static void parseJsonToStrings(const Json::Value& rootValue, const std::string& rootKey,
                                   std::map<std::string, Json::Value>& result);

    /* uWS specific handlers*/
    void handleError(void* user);
    void handleTransfer(uWS::WebSocket<uWS::CLIENT>* ws);
    void handleMessage(uWS::WebSocket<uWS::CLIENT>* ws, char* c, size_t t, uWS::OpCode op);
    void handleConnection(uWS::WebSocket<uWS::CLIENT>* ws, uWS::HttpRequest req);
    void handleDisconnection(uWS::WebSocket<uWS::CLIENT>* ws, int code, char* message,
                             size_t length);

    int mProtocolErrorCount {0};
    int mRequestId {0};
    bool recoverSubscribe {false};
    std::atomic<bool> mIsActive {false};
    ConnectionHandler mServerConnectionHandler {nullptr};
    uWS::WebSocket<uWS::CLIENT>* mWs {nullptr};
    static constexpr const char* const kDefaultVisUri {"wss://wwwivi:8088"};
    std::atomic<ConnState> mConnectedState {ConnState::STATE_DISCONNECTED};

    std::string mUri;
    uWS::Hub mHub;
    mutable std::mutex mLock;
    std::map<size_t, std::promise<Status>> mRequesIdToPromise;
    std::thread mThread;
    // Mapping of requestId to subscribe handler
    std::map<int, SubscriptionRequest> mSubscriptionObservers;
    // Mapping of subscribeId to subscribe handler
    std::map<int, SubscriptionRequest> mObservers;
    // Some promises for whose client futures are waiting
    std::map<int, std::promise<WMessageResult>> mPromised;
#ifdef RECOVER_SUBSCRIBE
    // Mapping of subscribeId to subscribe handler that need to be recovered
    std::map<int, SubscriptionRequest> mRecoverObservers;
#endif

    static const int kMaxBufferLength {1024};
    static const int kMaxWsMessageDelaySec {16};
    static const int kSleepTimeAfterErrorUs {100000};

    static constexpr const char* requestTemplateGet =
        "{\
            \"action\": \"get\",\
            \"path\": \"%s\",\
            \"requestId\": \"%d\"\
    }";

    static constexpr const char* requestTemplateSubscribe =
        "{\
            \"action\": \"subscribe\",\
            \"path\": \"%s\",\
            \"requestId\": \"%d\"\
    }";

    static constexpr const char* requestTemplateUnsubscribe =
        "{\
            \"action\": \"unsubscribe\",\
            \"subscriptionId\": \"%s\",\
            \"requestId\": \"%d\"\
    }";

    static constexpr const char* requestTemplateUnsubscribeAll =
        "{\
            \"action\": \"%s\",\
            \"requestId\": \"%d\"\
    }";

    static constexpr const char* requestTemplateSet =
        "{\
            \"action\": \"set\",\
            \"path\": \"%s\",\
            \"value\": %s,\
            \"requestId\": \"%d\"\
    }";


 public:
    static constexpr const char* const kAllTag {"*"};
    static constexpr const char* const kValueTag {"value"};
    static constexpr const char* const kTimestampTag {"timestamp"};
    static constexpr const char* const kActionTag {"action"};
    static constexpr const char* const kErrorTag {"error"};
    static constexpr const char* const kRequestIdTag {"requestId"};
    static constexpr const char* const kSubscriptionIdTag {"subscriptionId"};
    static constexpr const char* const kActionGetTag {"get"};
    static constexpr const char* const kActionSetTag {"set"};
    static constexpr const char* const kActionSubscribeTag {"subscribe"};
    static constexpr const char* const kActionSubscriptionTag {"subscription"};
    static constexpr const char* const kActionUnsubscribeTag {"unsubscribe"};
    static constexpr const char* const kActionUnsubscribeAllTag {"unsubscribeAll"};
};

}  // namespace epam

#endif /* COMMON_INCLUDE_VHAL_V2_0_VISCLIENT_H_ */
