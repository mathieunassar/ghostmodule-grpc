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

#ifndef GHOST_INTERNAL_NETWORK_RPCFINISH_HPP
#define GHOST_INTERNAL_NETWORK_RPCFINISH_HPP

#include <memory>

#include "RPCOperation.hpp"

namespace ghost
{
namespace internal
{
/**
 *	RPC Operation used by client calls to end the RPC.
 *	Gracefully shuts down the communication between the client and the server,
 *	and switches the RPC state to finish on completion.
 */
template <typename ReaderWriter, typename ContextType>
class RPCFinish : public RPCOperation<ReaderWriter, ContextType>
{
public:
	RPCFinish(std::weak_ptr<RPC<ReaderWriter, ContextType>> parent,
		  const grpc::Status& status = grpc::Status::CANCELLED);

	const grpc::Status& getStatus() const;

protected:
	bool initiateOperation() override;
	void onOperationSucceeded() override;
	void onOperationFailed() override;

private:
	grpc::Status _status;
};

/////////////////////////// Template definition ///////////////////////////

template <typename ReaderWriter, typename ContextType>
RPCFinish<ReaderWriter, ContextType>::RPCFinish(std::weak_ptr<RPC<ReaderWriter, ContextType>> parent,
						const grpc::Status& status)
    : RPCOperation<ReaderWriter, ContextType>(parent), _status(status)
{
}

template <typename ReaderWriter, typename ContextType>
bool RPCFinish<ReaderWriter, ContextType>::initiateOperation()
{
	auto rpc = RPCOperation<ReaderWriter, ContextType>::_rpc.lock();
	if (!rpc) return false;

	rpc->getContext()->TryCancel();
	rpc->getClient()->Finish(&_status, &(RPCOperation<ReaderWriter, ContextType>::_operationCompletedCallback));
	return true;
}

template <typename ReaderWriter, typename ContextType>
void RPCFinish<ReaderWriter, ContextType>::onOperationSucceeded()
{
	auto rpc = RPCOperation<ReaderWriter, ContextType>::_rpc.lock();
	if (!rpc) return;

	rpc->getStateMachine().setState(RPCStateMachine::FINISHED);
}

template <typename ReaderWriter, typename ContextType>
void RPCFinish<ReaderWriter, ContextType>::onOperationFailed()
{
	auto rpc = RPCOperation<ReaderWriter, ContextType>::_rpc.lock();
	if (!rpc) return;

	rpc->getStateMachine().setState(RPCStateMachine::FINISHED);
}

template <typename ReaderWriter, typename ContextType>
const grpc::Status& RPCFinish<ReaderWriter, ContextType>::getStatus() const
{
	return _status;
}
} // namespace internal
} // namespace ghost

#endif // GHOST_INTERNAL_NETWORK_RPCCONNECT_HPP
