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

#include "ClientManager.hpp"

#include <list>

#include "RemoteClientGRPC.hpp"

using namespace ghost::internal;

ClientManager::ClientManager(const std::shared_ptr<ghost::ThreadPool>& threadPool) : _threadPool(threadPool)
{
}

ClientManager::~ClientManager()
{
	deleteAllClients();
}

void ClientManager::start()
{
	if (!_executor)
	{
		_executor = _threadPool->makeScheduledExecutor();
		_executor->scheduleAtFixedRate(std::bind(&ClientManager::manageClients, this),
					       std::chrono::milliseconds(100));
	}
}

void ClientManager::stop()
{
	stopClients();

	if (_executor) _executor->stop();

	// do not delete the clients -> this might be called by a client's thread shutting down the server. In that
	// case, its executor thread would try to join itself
}

void ClientManager::addClient(std::shared_ptr<RemoteClientGRPC> client)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_allClients.push_back(client);
}

void ClientManager::stopClients()
{
	std::deque<std::shared_ptr<RemoteClientGRPC>> allClients;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		allClients = _allClients;
	}

	for (auto it = allClients.begin(); it != allClients.end(); ++it) (*it)->stop();
}

void ClientManager::shutdownClients()
{
	std::deque<std::shared_ptr<RemoteClientGRPC>> allClients;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		allClients = _allClients;
	}

	for (auto it = allClients.begin(); it != allClients.end(); ++it) (*it)->shutdown();
}

void ClientManager::deleteDisposableClients()
{
	std::list<std::shared_ptr<RemoteClientGRPC>> clientsToStop;
	{
		std::lock_guard<std::mutex> lock(_mutex);
		auto it = _allClients.begin();
		while (it != _allClients.end())
		{
			// remember the client to delete it once the mutex is released
			if (!(*it)->isRunning() && it->use_count() == 1)
			{
				clientsToStop.push_back(*it);
				it = _allClients.erase(it);
			}
			else
				++it;
		}
	}

	// stop the clients marked above
	for (auto& clientToStop : clientsToStop) clientToStop->getRPC()->dispose();
}

void ClientManager::deleteAllClients()
{
	for (auto it = _allClients.begin(); it != _allClients.end(); ++it)
	{
		(*it)->getRPC()->dispose();
	}
	_allClients.clear();
}

void ClientManager::manageClients()
{
	// stop, dispose grpc and delete all clients whose shared_ptr has a counter of 1
	deleteDisposableClients();
}
