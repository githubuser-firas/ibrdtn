/*
 * TCPConvergenceLayer.h
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

#ifndef TCPCONVERGENCELAYER_H_
#define TCPCONVERGENCELAYER_H_

#include "config.h"
#include "Component.h"

#include "core/Node.h"
#include "core/EventReceiver.h"
#include "net/ConvergenceLayer.h"
#include "net/TCPConnection.h"
#include "net/DiscoveryService.h"
#include "net/DiscoveryServiceProvider.h"

#include <ibrcommon/link/LinkManager.h>
#include <ibrcommon/net/vsocket.h>
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/thread/Mutex.h>

#include <set>
#include <list>
#include <map>

namespace dtn
{
	namespace net
	{
		/**
		 * This class implement a ConvergenceLayer for TCP/IP.
		 * http://tools.ietf.org/html/draft-irtf-dtnrg-tcp-clayer-02
		 */
		class TCPConvergenceLayer : public dtn::daemon::IndependentComponent, public dtn::core::EventReceiver, public ConvergenceLayer, public DiscoveryServiceProvider, public ibrcommon::LinkManager::EventCallback
		{
			friend class TCPConnection;

			const static std::string TAG;
		public:
			/**
			 * Constructor
			 * @param[in] bind_addr The address to bind.
			 * @param[in] port The port to use.
			 */
			TCPConvergenceLayer();

			/**
			 * Destructor
			 */
			virtual ~TCPConvergenceLayer();

			/**
			 * Add an interface to this convergence layer
			 * @param net Interface to listen on
			 * @param port Port to listen on
			 */
			void add(const ibrcommon::vinterface &net, int port) throw ();

			/**
			 * Queue a new transmission job for this convergence layer.
			 * @param job
			 */
			void queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job);

			/**
			 * Open a connection to the given node.
			 * @param n
			 * @return
			 */
			void open(const dtn::core::Node &n);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			/**
			 * Returns the discovery protocol type
			 * @return
			 */
			dtn::core::Node::Protocol getDiscoveryProtocol() const;

			/**
			 * this method updates the given values
			 */
			void update(const ibrcommon::vinterface &iface, DiscoveryAnnouncement &announcement)
				throw(dtn::net::DiscoveryServiceProvider::NoServiceHereException);

			void eventNotify(const ibrcommon::LinkEvent &evt);

			/**
			 * @see EventReceiver::raiseEvent()
			 */
			void raiseEvent(const Event *evt) throw ();

		protected:
			void __cancellation() throw ();

			void componentUp() throw ();
			void componentRun() throw ();
			void componentDown() throw ();

		private:
			/**
			 * Listen to a specific interface
			 */
			void listen(const ibrcommon::vinterface &iface, int port) throw ();

			/**
			 * Remove a specific interface
			 */
			void unlisten(const ibrcommon::vinterface &iface) throw ();

			/**
			 * closes all active tcp connections
			 */
			void closeAll();

			/**
			 * Add a connection to the connection list.
			 * @param conn
			 */
			void connectionUp(TCPConnection *conn);

			/**
			 * Removes a connection from the connection list. This method is called by the
			 * connection object itself to cleanup on a disconnect.
			 * @param conn
			 */
			void connectionDown(TCPConnection *conn);

			static const int DEFAULT_PORT;

			ibrcommon::vsocket _vsocket;
			bool _vsocket_state;

			ibrcommon::Conditional _connections_cond;
			std::list<TCPConnection*> _connections;

			ibrcommon::Mutex _interface_lock;
			std::set<ibrcommon::vinterface> _interfaces;

			ibrcommon::Mutex _portmap_lock;
			std::map<ibrcommon::vinterface, unsigned int> _portmap;
			unsigned int _any_port;
		};
	}
}

#endif /* TCPCONVERGENCELAYER_H_ */
