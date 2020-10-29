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

#include "ConnectionStressTest.hpp"

#include <thread>

const std::string ConnectionStressTest::TEST_NAME = "ConnectionStress";

ConnectionStressTest::ConnectionStressTest(const std::shared_ptr<ghost::ThreadPool>& threadPool,
					   const std::shared_ptr<ghost::Logger>& logger)
    : Systemtest(threadPool, logger), _messageSentIndex(0)
{
}

bool ConnectionStressTest::setUp()
{
	_connectionManager = ghost::ConnectionManager::create();
	ghost::ConnectionGRPC::initialize(_connectionManager, _threadPool);
	_messageSentIndex = 0;
	_messageReceivedIndex.clear();

	// set up the configuration to use for the static pubsub
	ghost::ConnectionConfigurationGRPC configuration;
	configuration.setServerPortNumber(17000);
	configuration.setOperationBlocking(false);

	// create a publisher to send test messages
	auto publisher = _connectionManager->createPublisher(configuration);
	require(publisher.operator bool());
	if (!publisher) return false;

	// store the writer of the publisher to be able to send the test messages.
	_publisherWriter = publisher->getWriter<google::protobuf::StringValue>();
	require(_publisherWriter.operator bool());
	if (!_publisherWriter) return false;

	bool publisherStartResult = publisher->start();
	require(publisherStartResult);

	// create subscribers that will receive the publisher's messages
	bool subscriberStartResult = true;
	for (size_t i = 0; i < 1; i++)
	{
		_messageReceivedIndex.push_back(0);

		auto subscriber = _connectionManager->createSubscriber(configuration);
		require(subscriber.operator bool());
		if (!subscriber) return false;

		// register the handler
		auto messageHandler = subscriber->addMessageHandler();
		messageHandler->addHandler<google::protobuf::StringValue>(
		    std::bind(&ConnectionStressTest::messageHandler, this, std::placeholders::_1, i));

		subscriberStartResult = subscriberStartResult && subscriber->start();
		require(subscriberStartResult);
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(500));
	return publisherStartResult && subscriberStartResult;
}

void ConnectionStressTest::tearDown()
{
	_publisherWriter.reset();
	_connectionManager.reset();

	_messageReceivedIndex.clear();
}

bool ConnectionStressTest::run()
{
	auto state = getState();
	auto nextLog = std::chrono::steady_clock::now();

	while (state == State::EXECUTING && checkTestDuration())
	{
		if (_messageSentIndex % 100'000 != 0 || _messageSentIndex == _messageReceivedIndex[0])
		{
			// Send to the static publisher
			auto msg = google::protobuf::StringValue::default_instance();
			msg.set_value(std::to_string(_messageSentIndex));
			bool writeResult = _publisherWriter->write(msg);
			_messageSentIndex++;
			require(writeResult);

			if (nextLog < std::chrono::steady_clock::now())
			{
				GHOST_INFO(_logger) << "Sending messages: " << _messageSentIndex;
				nextLog = std::chrono::steady_clock::now() + std::chrono::seconds(1);
			}
		}
		else if (nextLog < std::chrono::steady_clock::now())
		{
			GHOST_INFO(_logger)
			    << "Waiting for the subscribers to receive the messages: " << _messageReceivedIndex[0]
			    << " of " << _messageSentIndex;
			nextLog = std::chrono::steady_clock::now() + std::chrono::seconds(1);
		}

		// create an assert method that can fail the test? throw an exception that is caught be
		// Systemtest::execute

		state = getState();
	}

	return true;
}

void ConnectionStressTest::onPrintSummary() const
{
	GHOST_INFO(_logger) << "Sent " << _messageSentIndex << " and received " << _messageReceivedIndex[0]
			    << " messages.";
}

bool ConnectionStressTest::messageHandler(const google::protobuf::StringValue& message, size_t subscriberId)
{
	long long newIndex = std::stoll(message.value());
	require(newIndex >= _messageReceivedIndex[subscriberId]);
	_messageReceivedIndex[subscriberId]++;
	return true;
}

std::string ConnectionStressTest::getName() const
{
	return TEST_NAME;
}
