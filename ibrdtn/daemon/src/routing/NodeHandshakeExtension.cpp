/*
 * NodeHandshakeExtension.cpp
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

#include "routing/NodeHandshakeExtension.h"
#include "routing/NodeHandshakeEvent.h"

#include "core/NodeEvent.h"
#include "net/ConnectionEvent.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"

#include <ibrdtn/data/AgeBlock.h>
#include <ibrdtn/data/ScopeControlHopLimitBlock.h>
#include <ibrdtn/utils/Clock.h>

#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/thread/RWLock.h>
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace routing
	{
		const std::string NodeHandshakeExtension::TAG = "NodeHandshakeExtension";

		NodeHandshakeExtension::NodeHandshakeExtension()
		 : _endpoint(*this)
		{
		}

		NodeHandshakeExtension::~NodeHandshakeExtension()
		{
		}

		void NodeHandshakeExtension::requestHandshake(const dtn::data::EID&, NodeHandshake &request) const
		{
			request.addRequest(BloomFilterPurgeVector::identifier);
		}

		void NodeHandshakeExtension::responseHandshake(const dtn::data::EID&, const NodeHandshake &request, NodeHandshake &answer)
		{
			if (request.hasRequest(BloomFilterSummaryVector::identifier))
			{
				// add own summary vector to the message
				const dtn::data::BundleSet vec = (**this).getKnownBundles();

				// create an item
				BloomFilterSummaryVector *item = new BloomFilterSummaryVector(vec);

				// add it to the handshake
				answer.addItem(item);
			}

			if (request.hasRequest(BloomFilterPurgeVector::identifier))
			{
				// add own purge vector to the message
				const dtn::data::BundleSet vec = (**this).getPurgedBundles();

				// create an item
				BloomFilterPurgeVector *item = new BloomFilterPurgeVector(vec);

				// add it to the handshake
				answer.addItem(item);
			}
		}

		void NodeHandshakeExtension::processHandshake(const dtn::data::EID &source, NodeHandshake &answer)
		{
			try {
				const BloomFilterSummaryVector bfsv = answer.get<BloomFilterSummaryVector>();

				IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 10) << "summary vector received from " << source.getString() << IBRCOMMON_LOGGER_ENDL;

				// get the summary vector (bloomfilter) of this ECM
				const ibrcommon::BloomFilter &filter = bfsv.getVector().getBloomFilter();

				/**
				 * Update the neighbor database with the received filter.
				 * The filter was sent by the owner, so we assign the contained summary vector to
				 * the EID of the sender of this bundle.
				 */
				NeighborDatabase &db = (**this).getNeighborDB();
				ibrcommon::MutexLock l(db);
				db.create(source.getNode()).update(filter, answer.getLifetime());
			} catch (std::exception&) { };

			try {
				const BloomFilterPurgeVector bfpv = answer.get<BloomFilterPurgeVector>();

				IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 10) << "purge vector received from " << source.getString() << IBRCOMMON_LOGGER_ENDL;

				// get the purge vector (bloomfilter) of this ECM
				const ibrcommon::BloomFilter &purge = bfpv.getVector().getBloomFilter();

				dtn::storage::BundleStorage &storage = (**this).getStorage();

				while (true)
				{
					// delete bundles in the purge vector
					const dtn::data::MetaBundle meta = storage.remove(purge);

					if (meta.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
					{
						// log the purged bundle
						IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 10) << "bundle purged: " << meta.toString() << IBRCOMMON_LOGGER_ENDL;

						// gen a report
						dtn::core::BundleEvent::raise(meta, dtn::core::BUNDLE_DELETED, StatusReportBlock::NO_ADDITIONAL_INFORMATION);

						// add this bundle to the own purge vector
						(**this).addPurgedBundle(meta);
					}
					else
					{
						IBRCOMMON_LOGGER_TAG(NodeHandshakeExtension::TAG, warning) << source.getString() << " requested to purge a bundle with a non-singleton destination: " << meta.toString() << IBRCOMMON_LOGGER_ENDL;
					}
				}
			} catch (std::exception&) { };
		}

		void NodeHandshakeExtension::doHandshake(const dtn::data::EID &eid)
		{
			_endpoint.query(eid);
		}

		void NodeHandshakeExtension::notify(const dtn::core::Event *evt) throw ()
		{
			// If a new neighbor comes available, send him a request for the summary vector
			// If a neighbor went away we can free the stored summary vector
			try {
				const dtn::core::NodeEvent &nodeevent = dynamic_cast<const dtn::core::NodeEvent&>(*evt);
				const dtn::core::Node &n = nodeevent.getNode();

				if (nodeevent.getAction() == NODE_UNAVAILABLE)
				{
					// remove the item from the blacklist
					_endpoint.removeFromBlacklist(n.getEID());
				}

				return;
			} catch (const std::bad_cast&) { };
		}

		NodeHandshakeExtension::HandshakeEndpoint::HandshakeEndpoint(NodeHandshakeExtension &callback)
		 : _callback(callback)
		{
			AbstractWorker::initialize("/routing", 50, true);
		}

		NodeHandshakeExtension::HandshakeEndpoint::~HandshakeEndpoint()
		{
		}

		void NodeHandshakeExtension::HandshakeEndpoint::callbackBundleReceived(const Bundle &b)
		{
			_callback.processHandshake(b);
		}

		void NodeHandshakeExtension::HandshakeEndpoint::send(const dtn::data::Bundle &b)
		{
			transmit(b);
		}

		void NodeHandshakeExtension::HandshakeEndpoint::removeFromBlacklist(const dtn::data::EID &eid)
		{
			ibrcommon::MutexLock l(_blacklist_lock);
			_blacklist.erase(eid);
		}

		void NodeHandshakeExtension::HandshakeEndpoint::query(const dtn::data::EID &origin)
		{
			{
				ibrcommon::MutexLock l(_blacklist_lock);
				// only query once each 60 seconds
				if (_blacklist[origin] > dtn::utils::Clock::getUnixTimestamp()) return;
				_blacklist[origin] = dtn::utils::Clock::getUnixTimestamp() + 60;
			}

			// create a new request for the summary vector of the neighbor
			NodeHandshake request(NodeHandshake::HANDSHAKE_REQUEST);

			// walk through all extensions to generate a request
			(*_callback).requestHandshake(origin, request);

			IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 15) << "handshake query from " << origin.getString() << ": " << request.toString() << IBRCOMMON_LOGGER_ENDL;

			// create a new bundle
			dtn::data::Bundle req;

			// set the source of the bundle
			req.source = getWorkerURI();

			// set the destination of the bundle
			req.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, true);

			if (origin.isCompressable())
				req.destination = origin.add(origin.getDelimiter() + "50");
			else
				req.destination = origin.add(origin.getDelimiter() + "routing");

			// limit the lifetime to 60 seconds
			req.lifetime = 60;

			// set high priority
			req.set(dtn::data::PrimaryBlock::PRIORITY_BIT1, false);
			req.set(dtn::data::PrimaryBlock::PRIORITY_BIT2, true);

			dtn::data::PayloadBlock &p = req.push_back<PayloadBlock>();
			ibrcommon::BLOB::Reference ref = p.getBLOB();

			// serialize the request into the payload
			{
				ibrcommon::BLOB::iostream ios = ref.iostream();
				(*ios) << request;
			}

			// add a schl block
			dtn::data::ScopeControlHopLimitBlock &schl = req.push_front<dtn::data::ScopeControlHopLimitBlock>();
			schl.setLimit(1);

			// add an age block (to prevent expiring due to wrong clocks)
			req.push_front<dtn::data::AgeBlock>();

			// send the bundle
			transmit(req);
		}

		void NodeHandshakeExtension::processHandshake(const dtn::data::Bundle &bundle)
		{
			// read the ecm
			const dtn::data::PayloadBlock &p = bundle.find<dtn::data::PayloadBlock>();
			ibrcommon::BLOB::Reference ref = p.getBLOB();
			NodeHandshake handshake;

			// locked within this region
			{
				ibrcommon::BLOB::iostream s = ref.iostream();
				(*s) >> handshake;
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 15) << "handshake received from " << bundle.source.getString() << ": " << handshake.toString() << IBRCOMMON_LOGGER_ENDL;

			// if this is a request answer with an summary vector
			if (handshake.getType() == NodeHandshake::HANDSHAKE_REQUEST)
			{
				// create a new request for the summary vector of the neighbor
				NodeHandshake response(NodeHandshake::HANDSHAKE_RESPONSE);

				// lock the extension list during the processing
				(**this).responseHandshake(bundle.source, handshake, response);

				IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 15) << "handshake reply to " << bundle.source.getString() << ": " << response.toString() << IBRCOMMON_LOGGER_ENDL;

				// create a new bundle
				dtn::data::Bundle answer;

				// set the source of the bundle
				answer.source = _endpoint.getWorkerURI();

				// set the destination of the bundle
				answer.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, true);
				answer.destination = bundle.source;

				// limit the lifetime to 60 seconds
				answer.lifetime = 60;

				// set high priority
				answer.set(dtn::data::PrimaryBlock::PRIORITY_BIT1, false);
				answer.set(dtn::data::PrimaryBlock::PRIORITY_BIT2, true);

				dtn::data::PayloadBlock &p = answer.push_back<PayloadBlock>();
				ibrcommon::BLOB::Reference ref = p.getBLOB();

				// serialize the request into the payload
				{
					ibrcommon::BLOB::iostream ios = ref.iostream();
					(*ios) << response;
				}

				// add a schl block
				dtn::data::ScopeControlHopLimitBlock &schl = answer.push_front<dtn::data::ScopeControlHopLimitBlock>();
				schl.setLimit(1);

				// add an age block (to prevent expiring due to wrong clocks)
				answer.push_front<dtn::data::AgeBlock>();

				// transfer the bundle to the neighbor
				_endpoint.send(answer);

				// call handshake completed event
				NodeHandshakeEvent::raiseEvent( NodeHandshakeEvent::HANDSHAKE_REPLIED, bundle.source.getNode() );
			}
			else if (handshake.getType() == NodeHandshake::HANDSHAKE_RESPONSE)
			{
				// walk through all extensions to process the contents of the response
				(**this).processHandshake(bundle.source, handshake);

				// call handshake completed event
				NodeHandshakeEvent::raiseEvent( NodeHandshakeEvent::HANDSHAKE_COMPLETED, bundle.source.getNode() );
			}
		}
	} /* namespace routing */
} /* namespace dtn */
