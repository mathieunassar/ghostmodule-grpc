/*
 * Copyright 2020 Mathieu Nassar
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GHOST_INTERNAL_NETWORK_RPCREQUEST_HPP
#define GHOST_INTERNAL_NETWORK_RPCREQUEST_HPP

#include <grpcpp/completion_queue.h>

#include <functional>
#include <memory>

#include "RPCOperation.hpp"

namespace ghost
{
namespace internal
{
/**
 *	Operation used by a server connection to listen to client requests.
 *	On success, the corresponding RPC object represents the incoming client connection.
 */
template <typename ReaderWriter, typename ContextType, typename ServiceType>
class RPCRequest : public RPCOperation<ReaderWriter, ContextType>
{
public:
	RPCRequest(std::weak_ptr<RPC<ReaderWriter, ContextType>> parent, ServiceType* service,
		   grpc::CompletionQueue* rpcCompletionQueue, grpc::ServerCompletionQueue* completionQueue);

	void setConnectionCallback(const std::function<void()>& callback);

protected:
	bool initiateOperation() override;
	void onOperationSucceeded() override;
	void onOperationFailed() override;

private:
	ServiceType* _service;
	grpc::CompletionQueue* _rpcCompletionQueue;
	grpc::ServerCompletionQueue* _completionQueue;
	std::function<void()> _connectionCallback;
};

/////////////////////////// Template definition ///////////////////////////

template <typename ReaderWriter, typename ContextType, typename ServiceType>
RPCRequest<ReaderWriter, ContextType, ServiceType>::RPCRequest(std::weak_ptr<RPC<ReaderWriter, ContextType>> parent,
							       ServiceType* service,
							       grpc::CompletionQueue* rpcCompletionQueue,
							       grpc::ServerCompletionQueue* completionQueue)
    : RPCOperation<ReaderWriter, ContextType>(parent)
    , _service(service)
    , _rpcCompletionQueue(rpcCompletionQueue)
    , _completionQueue(completionQueue)
{
}

template <typename ReaderWriter, typename ContextType, typename ServiceType>
void RPCRequest<ReaderWriter, ContextType, ServiceType>::setConnectionCallback(const std::function<void()>& callback)
{
	_connectionCallback = callback;
}

template <typename ReaderWriter, typename ContextType, typename ServiceType>
bool RPCRequest<ReaderWriter, ContextType, ServiceType>::initiateOperation()
{
	if (!_connectionCallback) return false;

	auto rpc = RPCOperation<ReaderWriter, ContextType>::_rpc.lock();
	if (!rpc) return false;

	_service->Requestconnect(rpc->getContext().get(), rpc->getClient().get(), _rpcCompletionQueue, _completionQueue,
				 &(RPCOperation<ReaderWriter, ContextType>::_operationCompletedCallback));
	return true;
}

template <typename ReaderWriter, typename ContextType, typename ServiceType>
void RPCRequest<ReaderWriter, ContextType, ServiceType>::onOperationSucceeded()
{
	auto rpc = RPCOperation<ReaderWriter, ContextType>::_rpc.lock();
	if (!rpc) return;
	if (rpc->isFinished()) return; // nothing to do here

	rpc->getStateMachine().setState(RPCStateMachine::EXECUTING);
	if (_connectionCallback) _connectionCallback();
}

template <typename ReaderWriter, typename ContextType, typename ServiceType>
void RPCRequest<ReaderWriter, ContextType, ServiceType>::onOperationFailed()
{
	auto rpc = RPCOperation<ReaderWriter, ContextType>::_rpc.lock();
	if (!rpc) return;

	rpc->getStateMachine().setState(RPCStateMachine::FINISHED); // RPC could not start, finish it!
}

} // namespace internal
} // namespace ghost

#endif // GHOST_INTERNAL_NETWORK_RPCREQUEST_HPP
