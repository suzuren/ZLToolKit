﻿/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef Socket_h
#define Socket_h

#include <memory>
#include <string>
#include <deque>
#include <mutex>
#include <atomic>
#include <functional>
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "Poller/Timer.h"
#include "Network/sockutil.h"
#include "Thread/spin_mutex.h"
#include "Util/uv_errno.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Poller;
using namespace ZL::Thread;

namespace ZL {
namespace Network {

#if defined(MSG_NOSIGNAL)
#define FLAG_NOSIGNAL MSG_NOSIGNAL
#else
#define FLAG_NOSIGNAL 0
#endif //MSG_NOSIGNAL

#if defined(MSG_MORE)
#define FLAG_MORE MSG_MORE
#else
#define FLAG_MORE 0
#endif //MSG_MORE

#if defined(MSG_DONTWAIT)
#define FLAG_DONTWAIT MSG_DONTWAIT
#else
#define FLAG_DONTWAIT 0
#endif //MSG_DONTWAIT

#define TCP_DEFAULE_FLAGS (FLAG_NOSIGNAL | FLAG_DONTWAIT )
#define UDP_DEFAULE_FLAGS (FLAG_NOSIGNAL | FLAG_DONTWAIT)

#define MAX_SEND_PKT (256)

#if defined(__APPLE__)
  #import "TargetConditionals.h"
  #if TARGET_IPHONE_SIMULATOR
    #define OS_IPHONE
  #elif TARGET_OS_IPHONE
    #define OS_IPHONE
  #endif
#endif //__APPLE__

typedef enum {
	Err_success = 0, //成功
	Err_eof, //eof
	Err_timeout, //超时
	Err_refused,
	Err_dns,
	Err_other,
} ErrCode;


class SockException: public std::exception {
public:
	SockException(ErrCode _errCode = Err_success, const string &_errMsg = "") {
		errMsg = _errMsg;
		errCode = _errCode;
	}
	void reset(ErrCode _errCode, const string &_errMsg) {
		errMsg = _errMsg;
		errCode = _errCode;
	}
	virtual const char* what() const noexcept {
		return errMsg.c_str();
	}

	ErrCode getErrCode() const {
		return errCode;
	}
	operator bool() const{
		return errCode != Err_success;
	}
private:
	string errMsg;
	ErrCode errCode;
};
class SockFD
{
public:
	typedef std::shared_ptr<SockFD> Ptr;
	SockFD(int sock){
		_sock = sock;
	}
	virtual ~SockFD(){
        ::shutdown(_sock, SHUT_RDWR);
#if defined (OS_IPHONE)
        unsetSocketOfIOS(_sock);
#endif //OS_IPHONE
        int fd =  _sock;
        EventPoller::Instance().delEvent(fd,[fd](bool){
            close(fd);
        });
	}
	void setConnected(){
#if defined (OS_IPHONE)
		setSocketOfIOS(_sock);
#endif //OS_IPHONE
	}
	int rawFd() const{
		return _sock;
	}
private:
	int _sock;

#if defined (OS_IPHONE)
	void *readStream=NULL;
	void *writeStream=NULL;
	bool setSocketOfIOS(int socket);
	void unsetSocketOfIOS(int socket);
#endif //OS_IPHONE
};

class Socket: public std::enable_shared_from_this<Socket> {
public:
	class Buffer {
	public:
		typedef std::shared_ptr<Buffer> Ptr;
		Buffer(uint32_t size) {
			_size = size;
			_data = new char[size];
		}
		virtual ~Buffer() {
			delete[] _data;
		}
		const char *data() const {
			return _data;
		}
		uint32_t size() const {
			return _size;
		}
	private:
		friend class Socket;
		char *_data;
		uint32_t _size;
	};
	typedef std::shared_ptr<Socket> Ptr;
	typedef function<void(const Buffer::Ptr &buf, struct sockaddr *addr)> onReadCB;
	typedef function<void(const SockException &err)> onErrCB;
	typedef function<void(Socket::Ptr &sock)> onAcceptCB;
	typedef function<bool()> onFlush;

	Socket();
	virtual ~Socket();
	int rawFD() const{
		SockFD::Ptr sock;
		{
			lock_guard<spin_mutex> lck(_mtx_sockFd);
			sock = _sockFd;
		}
		if(!sock){
			return -1;
		}
		return sock->rawFd();
	}
	void connect(const string &url, uint16_t port,const onErrCB &connectCB, int timeoutSec = 5);
	bool listen(const uint16_t port, const char *localIp = "0.0.0.0", int backLog = 1024);
	bool bindUdpSock(const uint16_t port, const char *localIp = "0.0.0.0");

	void setOnRead(const onReadCB &cb);
	void setOnErr(const onErrCB &cb);
	void setOnAccept(const onAcceptCB &cb);
	void setOnFlush(const onFlush &cb);

	int send(const char *buf, int size = 0,int flags = TCP_DEFAULE_FLAGS);
	int send(const string &buf,int flags = TCP_DEFAULE_FLAGS);
	int send(string &&buf,int flags = TCP_DEFAULE_FLAGS);

	int sendTo(const char *buf, int size, struct sockaddr *peerAddr,int flags = UDP_DEFAULE_FLAGS);
	int sendTo(const string &buf, struct sockaddr *peerAddr,int flags = UDP_DEFAULE_FLAGS);
	int sendTo(string &&buf, struct sockaddr *peerAddr,int flags = UDP_DEFAULE_FLAGS);

	bool emitErr(const SockException &err);
	void enableRecv(bool enabled);

	string get_local_ip();
	uint16_t get_local_port();
	string get_peer_ip();
	uint16_t get_peer_port();

	void setSendPktSize(uint32_t iPktSize){
		_iMaxSendPktSize = iPktSize;
	}
    void setShouldDropPacket(bool dropPacket){
        _shouldDropPacket = dropPacket;
    }
private:
    class Packet
    {
    private:
        Packet(const Packet &that) = delete;
        Packet(Packet &&that) = delete;
        Packet &operator=(const Packet &that) = delete;
        Packet &operator=(Packet &&that) = delete;
    public:
		typedef std::shared_ptr<Packet> Ptr;
        Packet(const char *data,int len):_data(data,len){}
        Packet(const string &data):_data(data){}
        Packet(string &&data):_data(std::move(data)){}
        ~Packet(){  if(_addr){ delete _addr; } }

        void setAddr(const struct sockaddr *addr){
            if(_addr){
                *_addr = *addr;
            }else{
                _addr = new struct sockaddr(*addr);
            }
        }
        void setFlag(int flag){
            _flag = flag;
        }
        int send(int fd){
            int n;
            do {
                if(_addr){
                    n = ::sendto(fd, _data.data(), _data.size(), _flag, _addr, sizeof(struct sockaddr));
                }else{
                    n = ::send(fd, _data.data(), _data.size(), _flag);
                }
            } while (-1 == n && UV_EINTR == get_uv_error(true));

            if(n >= _data.size()){
                //全部发送成功
                _data.clear();
            }else if(n > 0) {
                //部分发送成功
                _data.erase(0, n);
            }
            return n;
        }
        int empty() const{
            return _data.empty();
        }
    private:
        struct sockaddr *_addr = nullptr;
        string _data;
        int _flag;
    };
private:
 	mutable spin_mutex _mtx_sockFd;
	SockFD::Ptr _sockFd;
	recursive_mutex _mtx_sendBuf;
	deque<Packet::Ptr> _sendPktBuf;
	/////////////////////
	std::shared_ptr<Timer> _conTimer;
	spin_mutex _mtx_read;
	spin_mutex _mtx_err;
	spin_mutex _mtx_accept;
	spin_mutex _mtx_flush;
	onReadCB _readCB;
	onErrCB _errCB;
	onAcceptCB _acceptCB;
	onFlush _flushCB;
	Ticker _flushTicker;
    uint32_t _iMaxSendPktSize = MAX_SEND_PKT;
    atomic<bool> _enableRecv;
    //默认网络底层可以主动丢包
    bool _shouldDropPacket = true;

	void closeSock();
	bool setPeerSock(int fd);
	bool attachEvent(const SockFD::Ptr &pSock,bool isUdp = false);

	int onAccept(const SockFD::Ptr &pSock,int event);
	int onRead(const SockFD::Ptr &pSock,bool mayEof=true);
	void onError(const SockFD::Ptr &pSock);
	int realSend(const string &buf, struct sockaddr *peerAddr,int flags,bool moveAble = false);
	bool onWrite(const SockFD::Ptr &pSock, bool bMainThread);
	void onConnected(const SockFD::Ptr &pSock, const onErrCB &connectCB);
	void onFlushed(const SockFD::Ptr &pSock);

	void startWriteEvent(const SockFD::Ptr &pSock);
	void stopWriteEvent(const SockFD::Ptr &pSock);
	bool sendTimeout();
	SockFD::Ptr makeSock(int sock){
		return std::make_shared<SockFD>(sock);
	}
	static SockException getSockErr(const SockFD::Ptr &pSock,bool tryErrno=true);

};

}  // namespace Network
}  // namespace ZL

#endif /* Socket_h */
