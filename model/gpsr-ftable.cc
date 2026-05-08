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

#include "gpsr-ftable.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("GpsrFlowTable");

namespace ns3 {
namespace gpsr {

bool FlowKey::operator==(const FlowKey& other) const
{
    return source == other.source && destination == other.destination && flowId == other.flowId;
}

std::size_t FlowKeyHash::operator()(const FlowKey& k) const
{
    return std::hash<uint32_t>()(k.source.Get()) ^
           (std::hash<uint32_t>()(k.destination.Get()) << 1) ^
           (std::hash<uint32_t>()(k.flowId) << 2);
}

FlowTable::FlowTable(Time cacheTimeout) : m_cacheTimeout(cacheTimeout)
{
}

bool FlowTable::Get(const FlowKey& key, FlowValue& value)
{
    auto it = m_table.find(key);
    if (it != m_table.end())
    {
        if (Simulator::Now() < it->second.timestamp + m_cacheTimeout)
        {
            value = it->second;
            NS_LOG_DEBUG("Flow cache HIT for flow " << key.source << " -> " << key.destination << " (id " << key.flowId << ")");
            return true;
        }
        else
        {
            NS_LOG_DEBUG("Flow cache EXPIRED for flow " << key.source << " -> " << key.destination << " (id " << key.flowId << ")");
            m_table.erase(it);
        }
    }
    NS_LOG_DEBUG("Flow cache MISS for flow " << key.source << " -> " << key.destination << " (id " << key.flowId << ")");
    return false;
}

void FlowTable::Set(const FlowKey& key, const FlowValue& value)
{
    NS_LOG_DEBUG("Flow cache SET for flow " << key.source << " -> " << key.destination << " (id " << key.flowId << "). Next hop: " << value.nextHop);
    m_table[key] = value;
}

void FlowTable::InvalidateEntriesFor(Ipv4Address neighbor)
{
    NS_LOG_DEBUG("Invalidating cache entries for neighbor " << neighbor);
    for (auto it = m_table.begin(); it != m_table.end();)
    {
        if (it->second.nextHop == neighbor)
        {
            NS_LOG_DEBUG("Invalidating flow " << it->first.source << " -> " << it->first.destination << " (id " << it->first.flowId << ")");
            it = m_table.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void FlowTable::Purge()
{
    if (m_cacheTimeout == Time(0))
    {
        return;
    }

    for (auto it = m_table.begin(); it != m_table.end();)
    {
        if (Simulator::Now() > it->second.timestamp + m_cacheTimeout)
        {
            it = m_table.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void FlowTable::SetCacheTimeout(Time timeout)
{
    m_cacheTimeout = timeout;
}

} // namespace gpsr
} // namespace ns3
