/****************************************************************************
 Copyright (c) 2012      greathqy
 Copyright (c) 2012      cocos2d-x.org
 Copyright (c) 2013-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 Copyright (c) 2021 Bytedance Inc.

 https://axmolengine.github.io/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#ifndef YASIO__EXT_HTTPCLIENT_H
#define YASIO__EXT_HTTPCLIENT_H

#include <thread>
#include <condition_variable>

#include "yasio_http/HttpRequest.h"
#include "yasio_http/HttpResponse.h"
#include "yasio_http/HttpCookie.h"
#include "yasio_http/Uri.h"
#include "yasio_http/utils/concurrent_deque.h"

#include "yasio/yasio_fwd.hpp"
#include "yasio/string_view.hpp"

/**
 * @addtogroup network
 * @{
 */

namespace yasio_ext
{

namespace network
{
/** Singleton that handles asynchronous http requests.
 *
 * Once the request completed, a callback will issued in main thread when it provided during make request.
 *
 * @lua NA
 */
class HttpClient
{
public:
    /**
     * How many requests could be perform concurrency.
     */
    static const int MAX_CHANNELS       = 21;

    /**
     * Get instance of HttpClient.
     *
     * @return the instance of HttpClient.
     */
    static HttpClient* getInstance();

    /**
     * Release the instance of HttpClient.
     */
    static void destroyInstance();

    /**
     * Enable cookie support.
     *
     * @param cookieFile the filepath of cookie file, must be writable full path
     */
    void enableCookies(cxx17::string_view cookieFile);

    /**
     * Get the cookie filename
     *
     * @return the cookie filename
     */
    cxx17::string_view getCookieFilename();

    /**
     * Set root certificate path for SSL verification.
     *
     * @param caFile a full path of root certificate.if it is empty, SSL verification is disabled.
     */
    void setSSLVerification(cxx17::string_view caFile);

    /**
     * Get the ssl CA filename
     *
     * @return the ssl CA filename
     */
    cxx17::string_view getSSLVerification();

    /**
     * Send http request concurrently, non-blocking
     *
     * @param request a HttpRequest object, which includes url, response callback etc.
                      please make sure request->_requestData is clear before calling "send" here.
     * @notes for post data
     *   a. By default will fill Content-Type: application/x-www-form-urlencoded;charset=UTF-8
     *   b. You can specific content-type at custom header, such as:
     *      std::vector<std::string> headers = {"Content-Type: application/json;charset=UTF-8"};
     *      request->setHeaders(headers);
     *   c. other content type, please see:
     *      https://stackoverflow.com/questions/23714383/what-are-all-the-possible-values-for-http-content-type-header
     */
    bool send(HttpRequest* request);

    /**
     * Send http request sync, will block caller thread until request finished.
     * @remark  Caller must call release manually when the response never been used.
     */
    HttpResponse* sendSync(HttpRequest* request);

    /**
     * Set the timeout value for connecting.
     *
     * @param value the timeout value for connecting.
     */
    void setTimeoutForConnect(int value);

    /**
     * Get the timeout value for connecting.
     *
     * @return int the timeout value for connecting.
     */
    int getTimeoutForConnect();

    /**
     * Set the timeout value for reading.
     *
     * @param value the timeout value for reading.
     */
    void setTimeoutForRead(int value);

    /**
     * Get the timeout value for reading.
     *
     * @return int the timeout value for reading.
     */
    int getTimeoutForRead();

    HttpCookie* getCookie() const { return _cookie; }

    std::recursive_mutex& getCookieFileMutex() { return _cookieFileMutex; }

    std::recursive_mutex& getSSLCaFileMutex() { return _sslCaFileMutex; }

    typedef std::function<bool(HttpResponse*)> ClearResponsePredicate;

    /**
     * Clears the pending & finished http response
     */
    void clearResponseQueue();

    /**
     * Clears the pending http response
     */
    void clearPendingResponseQueue();

    /**
     * Clears the finished http response
     */
    void clearFinishedResponseQueue();

    /**
     Sets a predicate function that is going to be called to determine if we proceed
    * each of the pending requests
    *
    * @param cb predicate function that will be called
    */
    void setClearResponsePredicate(ClearResponsePredicate predicate) { _clearResponsePredicate = predicate; }

    void setDispatchOnWorkThread(bool bVal);
    bool isDispatchOnWorkThread() const { return _dispatchOnWorkThread; }

    /*
     * When the device network status chagned, you should invoke this function
     */
    void handleNetworkStatusChanged();

    /*
     * Sets custom dns server list:
     * format: "xxx.xxx.xxx.xxx[:port],xxx.xxx.xxx.xxx[:port]
     */
    void setNameServers(cxx17::string_view servers);

    yasio::io_service* getInternalService();

    /*
     * If setDispatchOnWorkThread(false), you needs to invoke this API to dispatch http response
     * on the caller thread
     */
    void tickInput();

    /*
     * The urlEncode API
     * Perform urlEncode for query params always
     * Perform urlEncode for post data when content-type is: application/x-www-form-urlencoded
     */
    static std::string urlEncode(cxx17::string_view s);

    static std::string urlDecode(cxx17::string_view st);

private:
    HttpClient();
    virtual ~HttpClient();

    void processResponse(HttpResponse* response, int channelIndex);

    int tryTakeAvailChannel();

    void handleNetworkEvent(yasio::io_event* event);

    void handleNetworkEOF(HttpResponse* response, yasio::io_channel* channel, int internalErrorCode);

    // void tickInput();

    void finishResponse(HttpResponse* response);

    void invokeResposneCallbackAndRelease(HttpResponse* response);

private:
    bool _isInited;

    yasio::io_service* _service;

    bool _dispatchOnWorkThread;

    int _timeoutForConnect;
    std::recursive_mutex _timeoutForConnectMutex;

    int _timeoutForRead;
    std::recursive_mutex _timeoutForReadMutex;

    concurrent_deque<HttpResponse*> _pendingResponseQueue;
    concurrent_deque<HttpResponse*> _finishedResponseQueue;

    concurrent_deque<int> _availChannelQueue;

    std::string _cookieFilename;
    std::recursive_mutex _cookieFileMutex;

    std::string _sslCaFilename;
    std::recursive_mutex _sslCaFileMutex;

    HttpCookie* _cookie;

    ClearResponsePredicate _clearResponsePredicate;
};

}  // namespace network

}  // namespace yasio_ext

// end group
/// @}

#endif  //__CCHTTPCLIENT_H__
