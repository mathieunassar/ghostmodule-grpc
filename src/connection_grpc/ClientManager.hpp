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

#ifndef GHOST_INTERNAL_NETWORK_CLIENTMANAGER_HPP
#define GHOST_INTERNAL_NETWORK_CLIENTMANAGER_HPP

#include <atomic>
#include <deque>
#include <ghost/module/ThreadPool.hpp>
#include <memory>
#include <mutex>

namespace ghost
{
namespace internal
{
class RemoteClientGRPC;

/**
 *	Manager for gRPC clients represented by a RemoteClientGRPC (internally containing
 *	ghost::internal::IncomingRPC objects).
 *	Acts as a collection of clients and can shut down all clients.
 *	Periodically deletes clients that are shut down and not used anymore.
 */
class ClientManager
{
public:
	ClientManager(const std::shared_ptr<ghost::ThreadPool>& threadPool);
	~ClientManager();

	/// Starts a thread periodically deleting old clients
	void start();
	/// stops the thread periodically deleting old clients, and deletes all the clients after disposing them
	/// this call might be blocking while the clients are disposing
	void stop();

	/// Adds a client to the manager
	void addClient(std::shared_ptr<RemoteClientGRPC> client);
	/// Stops currently running clients.
	void stopClients();
	void shutdownClients();

private:
	/// dispose and delete clients that are in finished state and owned solely by this manager
	void deleteDisposableClients();
	/// Deletes all managed clients. Should not be called concurrently with "add"
	void deleteAllClients();
	/// loops over the managed clients and tries to delte unused clients with "deleteDisposableClients"
	void manageClients();

	std::shared_ptr<ghost::ThreadPool> _threadPool;
	std::shared_ptr<ghost::ScheduledExecutor> _executor;
	std::mutex _mutex;
	std::deque<std::shared_ptr<RemoteClientGRPC>> _allClients;
};
} // namespace internal
} // namespace ghost

#endif // GHOST_INTERNAL_NETWORK_CLIENTMANAGER_HPP
