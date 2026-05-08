/*
 *
 * Copyright (c) 2024 Author
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Author
 */

#ifndef GPSR_FTABLE_H
#define GPSR_FTABLE_H

#include "ns3/ipv4-address.h"
#include "ns3/simulator.h"
#include <unordered_map>
#include <vector>

namespace ns3 {
namespace gpsr {

// Key for the flow table
struct FlowKey {
    Ipv4Address source;
    Ipv4Address destination;
    uint32_t flowId;

    bool operator==(const FlowKey& other) const;
};

// Value for the flow table
struct FlowValue {
    Ipv4Address nextHop;
    Time timestamp;
};

// Hash function for FlowKey
struct FlowKeyHash {
    std::size_t operator()(const FlowKey& k) const;
};

/**
 * \ingroup gpsr
 * \brief Manages a cache of flows for GPSR
 */
class FlowTable
{
public:
  FlowTable(Time cacheTimeout);

  /// Check if a valid entry exists for a flow
  bool Get(const FlowKey& key, FlowValue& value);

  /// Add or update an entry in the table
  void Set(const FlowKey& key, const FlowValue& value);

  /// Remove entries for a specific next hop (neighbor)
  void InvalidateEntriesFor(Ipv4Address neighbor);

  /// Remove expired entries from the table
  void Purge();

  /// Set cache timeout
  void SetCacheTimeout(Time timeout);

private:
  std::unordered_map<FlowKey, FlowValue, FlowKeyHash> m_table;
  Time m_cacheTimeout;
};

} // namespace gpsr
} // namespace ns3

#endif // GPSR_FTABLE_H
