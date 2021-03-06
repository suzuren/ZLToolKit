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

#ifndef SERVER_SESSION_H_
#define SERVER_SESSION_H_
#include <memory>
#include "Socket.h"
#include "Util/logger.h"
#include "Util/mini.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

namespace ZL {
namespace Network {

class TcpSession: public std::enable_shared_from_this<TcpSession> {
public:
	TcpSession(const std::shared_ptr<ThreadPool> &_th, const Socket::Ptr &_sock) :
			sock(_sock), th(_th) {
		localIp = sock->get_local_ip();
		peerIp = sock->get_peer_ip();
		localPort = sock->get_local_port();
		peerPort = sock->get_peer_port();
	}
	virtual ~TcpSession() {
	}
	virtual void onRecv(const Socket::Buffer::Ptr &) =0;
	virtual void onError(const SockException &err) =0;
	virtual void onManager() =0;
    virtual void attachServer(const mINI &ini){};
	template <typename T>
	void async(T &&task) {
		th->async(std::forward<T>(task));
	}
	template <typename T>
	void async_first(T &&task) {
		th->async_first(std::forward<T>(task));
	}

protected:
	const string& getLocalIp() const {
		return localIp;
	}
	const string& getPeerIp() const {
		return peerIp;
	}
	uint16_t getLocalPort() const {
		return localPort;
	}
	uint16_t getPeerPort() const {
		return peerPort;
	}
	virtual void shutdown() {
		sock->emitErr(SockException(Err_other, "self shutdown"));
	}
	void safeShutdown(){
		std::weak_ptr<TcpSession> weakSelf = shared_from_this();
		async_first([weakSelf](){
			auto strongSelf = weakSelf.lock();
			if(strongSelf){
				strongSelf->shutdown();
			}
		});
	}
	virtual int send(const string &buf) {
		return sock->send(buf);
	}
	virtual int send(string &&buf) {
		return sock->send(std::move(buf));
	}
	virtual int send(const char *buf, int size) {
		return sock->send(buf, size);
	}

	Socket::Ptr sock;
private:
	std::shared_ptr<ThreadPool> th;
	string localIp;
	string peerIp;
	uint16_t localPort;
	uint16_t peerPort;
};


} /* namespace Session */
} /* namespace ZL */

#endif /* SERVER_SESSION_H_ */
