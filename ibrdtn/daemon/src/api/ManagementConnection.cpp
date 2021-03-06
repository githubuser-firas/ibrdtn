/*
 * ManagementConnection.cpp
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "config.h"
#include "Configuration.h"
#include "ManagementConnection.h"
#include "storage/BundleResult.h"
#include "core/BundleCore.h"
#include "core/GlobalEvent.h"
#include "routing/prophet/ProphetRoutingExtension.h"
#include "routing/prophet/DeliveryPredictabilityMap.h"

#include <ibrdtn/utils/Utils.h>

#include <ibrcommon/Logger.h>
#include <ibrcommon/link/LinkManager.h>
#include <ibrcommon/link/LinkEvent.h>
#include <ibrcommon/thread/RWLock.h>

namespace dtn
{
	namespace api
	{
		ManagementConnection::ManagementConnection(ClientHandler &client, ibrcommon::socketstream &stream)
		 : ProtocolHandler(client, stream)
		{
		}

		ManagementConnection::~ManagementConnection()
		{
		}

		void ManagementConnection::run()
		{
			std::string buffer = "";
			_stream << ClientHandler::API_STATUS_OK << " SWITCHED TO MANAGEMENT" << std::endl;

			// run as long the stream is ok
			while (_stream.good())
			{
				getline(_stream, buffer);

				if (buffer.length() == 0) continue;

				// search for '\r\n' and remove the '\r'
				std::string::reverse_iterator iter = buffer.rbegin();
				if ( (*iter) == '\r' ) buffer = buffer.substr(0, buffer.length() - 1);

				std::vector<std::string> cmd = dtn::utils::Utils::tokenize(" ", buffer);
				if (cmd.empty()) continue;

				if (cmd[0] == "exit")
				{
					// return to previous level
					break;
				}
				else
				{
					// forward to standard command set
					processCommand(cmd);
				}
			}
		}

		void ManagementConnection::finally()
		{
		}

		void ManagementConnection::setup()
		{
		}

		void ManagementConnection::__cancellation() throw ()
		{
		}

		void ManagementConnection::processCommand(const std::vector<std::string> &cmd)
		{
			class BundleFilter : public dtn::storage::BundleSelector
			{
			public:
				BundleFilter()
				{};

				virtual ~BundleFilter() {};

				virtual dtn::data::Size limit() const throw () { return 0; };

				virtual bool shouldAdd(const dtn::data::MetaBundle&) const throw (dtn::storage::BundleSelectorException)
				{
					return true;
				}
			};

			try {
				if (cmd[0] == "neighbor")
				{
					if (cmd.size() < 2) throw ibrcommon::Exception("not enough parameters");

					if (cmd[1] == "list")
					{
						const std::set<dtn::core::Node> nlist = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();

						_stream << ClientHandler::API_STATUS_OK << " NEIGHBOR LIST" << std::endl;
						for (std::set<dtn::core::Node>::const_iterator iter = nlist.begin(); iter != nlist.end(); ++iter)
						{
							_stream << (*iter).getEID().getString() << std::endl;
						}
						_stream << std::endl;
					}
					else
					{
						_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
					}
				}
				else if (cmd[0] == "interface")
				{
					if (cmd[1] == "address")
					{
						try {
							ibrcommon::LinkManager &lm = ibrcommon::LinkManager::getInstance();
							
							// the interface is defined as the 3rd parameter
							ibrcommon::vinterface iface(cmd[3]);
							
							// TODO: Throw out the 4th parameter. Previously used to define the
							// address family.

							// the new address is defined as 5th parameter
							ibrcommon::vaddress addr(cmd[5], "");

							if (cmd[2] == "add")
							{
								ibrcommon::LinkEvent evt(ibrcommon::LinkEvent::ACTION_ADDRESS_ADDED, iface, addr);
								lm.raiseEvent(evt);
								_stream << ClientHandler::API_STATUS_OK << " ADDRESS ADDED" << std::endl;
							}
							else if (cmd[2] == "del")
							{
								ibrcommon::LinkEvent evt(ibrcommon::LinkEvent::ACTION_ADDRESS_REMOVED, iface, addr);
								lm.raiseEvent(evt);
								_stream << ClientHandler::API_STATUS_OK << " ADDRESS REMOVED" << std::endl;
							}
							else
							{
								_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
							}
						} catch (const std::bad_cast&) {
							_stream << ClientHandler::API_STATUS_NOT_ALLOWED << " FEATURE NOT AVAILABLE" << std::endl;
						};
					}
					else
					{
						_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
					}
				}
				else if (cmd[0] == "connection")
				{
					// need to process the connection arguments
					// the arguments look like:
					// <eid> [tcp|udp|file] [add|del] <ip> <port> <global|local>
					dtn::core::Node n(cmd[1]);
					dtn::core::Node::Type t = dtn::core::Node::NODE_STATIC_GLOBAL;

					if (cmd.size() > 6) {
						if (cmd[6] == "global") {
							t = dtn::core::Node::NODE_STATIC_GLOBAL;
						} else {
							t = dtn::core::Node::NODE_STATIC_LOCAL;
						}
					}

					if (cmd[2] == "tcp")
					{
						if (cmd[3] == "add")
						{
							std::string uri = "ip=" + cmd[4] + ";port=" + cmd[5] + ";";
							n.add(dtn::core::Node::URI(t, dtn::core::Node::CONN_TCPIP, uri, 0, 10));
							dtn::core::BundleCore::getInstance().getConnectionManager().add(n);

							_stream << ClientHandler::API_STATUS_OK << " CONNECTION ADDED" << std::endl;
						}
						else if (cmd[3] == "del")
						{
							std::string uri = "ip=" + cmd[4] + ";port=" + cmd[5] + ";";
							n.add(dtn::core::Node::URI(t, dtn::core::Node::CONN_TCPIP, uri, 0, 10));
							dtn::core::BundleCore::getInstance().getConnectionManager().remove(n);

							_stream << ClientHandler::API_STATUS_OK << " CONNECTION REMOVED" << std::endl;
						}
					}
					else if (cmd[2] == "udp")
					{
						if (cmd[3] == "add")
						{
							std::string uri = "ip=" + cmd[4] + ";port=" + cmd[5] + ";";
							n.add(dtn::core::Node::URI(t, dtn::core::Node::CONN_UDPIP, uri, 0, 10));
							dtn::core::BundleCore::getInstance().getConnectionManager().add(n);

							_stream << ClientHandler::API_STATUS_OK << " CONNECTION ADDED" << std::endl;
						}
						else if (cmd[3] == "del")
						{
							std::string uri = "ip=" + cmd[4] + ";port=" + cmd[5] + ";";
							n.add(dtn::core::Node::URI(t, dtn::core::Node::CONN_UDPIP, uri, 0, 10));
							dtn::core::BundleCore::getInstance().getConnectionManager().remove(n);

							_stream << ClientHandler::API_STATUS_OK << " CONNECTION REMOVED" << std::endl;
						}
					}
					else if (cmd[2] == "file")
					{
						if (cmd[3] == "add")
						{
							n.add(dtn::core::Node::URI(dtn::core::Node::NODE_STATIC_LOCAL, dtn::core::Node::CONN_FILE, cmd[4], 0, 10));
							dtn::core::BundleCore::getInstance().getConnectionManager().add(n);

							_stream << ClientHandler::API_STATUS_OK << " CONNECTION ADDED" << std::endl;
						}
						else if (cmd[3] == "del")
						{
							n.add(dtn::core::Node::URI(dtn::core::Node::NODE_STATIC_LOCAL, dtn::core::Node::CONN_FILE, cmd[4], 0, 10));
							dtn::core::BundleCore::getInstance().getConnectionManager().remove(n);

							_stream << ClientHandler::API_STATUS_OK << " CONNECTION REMOVED" << std::endl;
						}
					}
				}
				else if (cmd[0] == "logcat")
				{
					ibrcommon::Logger::writeBuffer(_stream);
					_stream << std::endl;
				}
				else if (cmd[0] == "core")
				{
					if (cmd[1] == "shutdown")
					{
						// send shutdown signal
						dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_SHUTDOWN);
						_stream << ClientHandler::API_STATUS_OK << " SHUTDOWN" << std::endl;
					}
					else if (cmd[1] == "reload")
					{
						// send reload signal
						dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_RELOAD);
						_stream << ClientHandler::API_STATUS_OK << " RELOAD" << std::endl;
					}
					else if (cmd[1] == "powersave")
					{
						// send powersave signal
						dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_POWERSAVE);
						_stream << ClientHandler::API_STATUS_OK << " POWERSAVE" << std::endl;
					}
					else if (cmd[1] == "suspend")
					{
						// send suspend signal
						dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_SUSPEND);
						_stream << ClientHandler::API_STATUS_OK << " SUSPEND" << std::endl;
					}
					else if (cmd[1] == "wakeup")
					{
						// send wakeup signal
						dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_WAKEUP);
						_stream << ClientHandler::API_STATUS_OK << " WAKEUP" << std::endl;
					}
					else if (cmd[1] == "internet_off")
					{
						// send internet off signal
						dtn::core::BundleCore::getInstance().setGloballyConnected(false);
						_stream << ClientHandler::API_STATUS_OK << " INTERNET OFF" << std::endl;
					}
					else if (cmd[1] == "internet_on")
					{
						// send internet off signal
						dtn::core::BundleCore::getInstance().setGloballyConnected(true);
						_stream << ClientHandler::API_STATUS_OK << " INTERNET ON" << std::endl;
					}
				}
				else if (cmd[0] == "bundle")
				{
					if (cmd[1] == "list")
					{
						// get storage object
						dtn::core::BundleCore &bcore = dtn::core::BundleCore::getInstance();
						BundleFilter filter;

						_stream << ClientHandler::API_STATUS_OK << " BUNDLE LIST" << std::endl;
						dtn::storage::BundleResultList blist;
						
						try {
							bcore.getStorage().get(filter, blist);

							for (std::list<dtn::data::MetaBundle>::const_iterator iter = blist.begin(); iter != blist.end(); ++iter)
							{
								const dtn::data::MetaBundle &b = *iter;
								_stream << b.toString() << ";" << b.destination.getString() << ";" << std::endl;
							}
						} catch (const dtn::storage::NoBundleFoundException&) { }

						// last line empty
						_stream << std::endl;
					}
				}
				else if (cmd[0] == "routing")
				{
					if (cmd.size() < 3) throw ibrcommon::Exception("not enough parameters");

					if ( cmd[1] == "prophet" )
					{
						dtn::routing::BaseRouter &router = dtn::core::BundleCore::getInstance().getRouter();

						// lock the extension list during the processing
						ibrcommon::RWLock l(router.getExtensionMutex(), ibrcommon::RWMutex::LOCK_READONLY);

						const dtn::routing::BaseRouter::extension_list& routingExtensions = router.getExtensions();
						dtn::routing::BaseRouter::extension_list::const_iterator it;

						/* find the prophet extension in the BaseRouter */

						for(it = routingExtensions.begin(); it != routingExtensions.end(); ++it)
						{
							try
							{
								const dtn::routing::ProphetRoutingExtension& prophet_extension = dynamic_cast<const dtn::routing::ProphetRoutingExtension&>(**it);

								if ( cmd[2] == "info" ){
									ibrcommon::ThreadsafeReference<const dtn::routing::DeliveryPredictabilityMap> dp_map = prophet_extension.getDeliveryPredictabilityMap();

									_stream << ClientHandler::API_STATUS_OK << " ROUTING PROPHET INFO" << std::endl;
									_stream << *dp_map << std::endl;
								} else if ( cmd[2] == "acknowledgements" ) {
									ibrcommon::ThreadsafeReference<const dtn::routing::AcknowledgementSet> ack_set = prophet_extension.getAcknowledgementSet();

									_stream << ClientHandler::API_STATUS_OK << " ROUTING PROPHET ACKNOWLEDGEMENTS" << std::endl;
									for (dtn::routing::AcknowledgementSet::const_iterator iter = (*ack_set).begin(); iter != (*ack_set).end(); ++iter)
									{
										const dtn::data::MetaBundle &ack = (*iter);
										_stream << ack.toString() << " | " << ack.expiretime.toString() << std::endl;
									}
									_stream << std::endl;
								} else {
									throw ibrcommon::Exception("malformed command");
								}

								break;
							} catch (const std::bad_cast&) { }
						}
						if(it == routingExtensions.end())
						{
							/* no prophet routing extension found */
							_stream << ClientHandler::API_STATUS_NOT_ACCEPTABLE << " ROUTING PROPHET EXTENSION NOT FOUND" << std::endl;
						}
					} else {
						throw ibrcommon::Exception("malformed command");
					}
				}
				else
				{
					_stream << ClientHandler::API_STATUS_BAD_REQUEST << " UNKNOWN COMMAND" << std::endl;
				}
			} catch (const std::exception&) {
				_stream << ClientHandler::API_STATUS_BAD_REQUEST << " ERROR" << std::endl;
			}
		}
	} /* namespace api */
} /* namespace dtn */
