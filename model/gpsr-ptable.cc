#include "gpsr-ptable.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include <algorithm>

NS_LOG_COMPONENT_DEFINE ("GpsrTable");
namespace ns3 {
  namespace gpsr {

    /*
      GPSR position table
    */

    PositionTable::PositionTable(Time entryTime, uint8_t graphType) : m_entryLifeTime(entryTime), graphType(graphType)
    {
        m_txErrorCallback = MakeCallback(&PositionTable::ProcessTxError, this);
    }

    Time 
    PositionTable::GetEntryUpdateTime(Ipv4Address id)
    {
        if (id == Ipv4Address::GetZero())
        {
            return Time(Seconds(0));
        }
        
        std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i = m_table.find(id);
        return i->second.second;
    }

    /**
     * \brief Adds entry in position table
     */
    void 
    PositionTable::AddEntry(Ipv4Address id, Vector position)
    {
        std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i = m_table.find(id);
        if (i != m_table.end() || id.IsEqual(i->first))
        {
            m_table.erase(id);
            m_table.insert(std::make_pair(id, std::make_pair(position, Simulator::Now())));
            return;
        }
        
        m_table.insert(std::make_pair(id, std::make_pair(position, Simulator::Now())));
    }

    /**
     * \brief Deletes entry in position table
     */
    void PositionTable::DeleteEntry(Ipv4Address id)
    {
        m_table.erase(id);
    }

    /**
     * \brief Gets position from position table
     * \param id Ipv4Address to get position from
     * \return Position of that id or NULL if not known
     */
    Vector 
    PositionTable::GetPosition(Ipv4Address id)
    {
        NodeList::Iterator listEnd = NodeList::End();
        for (NodeList::Iterator i = NodeList::Begin(); i != listEnd; i++)
        {
            Ptr<Node> node = *i;
            if (node->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() == id)
            {
                return node->GetObject<MobilityModel>()->GetPosition();
            }
        }
        
        return PositionTable::GetInvalidPosition();
    }

    /**
     * \brief Checks if a node is a neighbour
     * \param id Ipv4Address of the node to check
     * \return True if the node is neighbour, false otherwise
     */
    bool
    PositionTable::isNeighbour(Ipv4Address id)
    {
       std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i = m_table.find(id);

        if (i != m_table.end() || id.IsEqual(i->first))
        {
            return true;
        }

        return false;
    }

    /**
     * \brief remove entries with expired lifetime
     */
    void 
    PositionTable::Purge()
    {
        if (m_table.empty())
        {
            return;
        }

        std::list<Ipv4Address> toErase;

        std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i = m_table.begin();
        std::map<Ipv4Address, std::pair<Vector, Time> >::iterator listEnd = m_table.end();
        
        for (; !(i == listEnd); i++)
        {
            if (m_entryLifeTime + GetEntryUpdateTime(i->first) <= Simulator::Now())
            {
                toErase.insert(toErase.begin(), i->first);
            }
        }

        toErase.unique();

        std::list<Ipv4Address>::iterator end = toErase.end();

        for (std::list<Ipv4Address>::iterator it = toErase.begin(); it != end; ++it)
        {
            m_table.erase(*it);
        }
    }

    /**
     * \brief clears all entries
     */
    void 
    PositionTable::Clear()
    {
        m_table.clear();
    }

    /**
     * \brief Gets next hop according to GPSR protocol
     * \param position the position of the destination node
     * \param nodePos the position of the node that has the packet
     * \return Ipv4Address of the next hop, Ipv4Address::GetZero() if no nighbour was found in greedy mode
     */
    Ipv4Address 
    PositionTable::BestNeighbor(Vector destPos, Vector nodePos)
    {
        Purge();

        double initialDistance = CalculateDistance(nodePos, destPos);

        if (m_table.empty())
        {
            NS_LOG_DEBUG("BestNeighbor table is empty; Position: " << destPos);
            return Ipv4Address::GetZero();
        }     //if table is empty(no neighbours)

        Ipv4Address bestFoundID = m_table.begin()->first;
        double bestFoundDistance = CalculateDistance(m_table.begin()->second.first, destPos);
        std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i;
        for (i = m_table.begin(); !(i == m_table.end()); i++)
        {
            if (bestFoundDistance > CalculateDistance(i->second.first, destPos))
            {
                bestFoundID = i->first;
                bestFoundDistance = CalculateDistance(i->second.first, destPos);
            }
        }

        if (initialDistance > bestFoundDistance)
          return bestFoundID;
        else
          return Ipv4Address::GetZero(); //so it enters Recovery-mode
    }

    std::vector<std::pair<Ipv4Address, Vector>>
    PositionTable::GetNeighbors()
    {
        std::vector<Ipv4Address> toErase;
        std::vector<std::pair<Ipv4Address, Vector>> neighbors;

        for (const auto& entry : m_table)
        {
            if (m_entryLifeTime + GetEntryUpdateTime(entry.first) <= Simulator::Now())
            {
                toErase.insert(toErase.begin(), entry.first);
            }

            else {

                neighbors.push_back({entry.first, entry.second.first});
            }
        }

        for (const auto& id : toErase)
        {
            m_table.erase(id);
        }

        return neighbors;
    }

    std::vector<std::pair<Ipv4Address, Vector>>
    PositionTable::GetGabrielNeighbors(const Vector& nodePos)
    {
        Purge();
        std::vector<std::pair<Ipv4Address, Vector>> gabrielNeighbors;

        for (auto itA = m_table.begin(); itA != m_table.end(); ++itA)
        {
            const Ipv4Address& aId = itA->first;
            const Vector& aPos = itA->second.first;

            if (aPos.x == nodePos.x && aPos.y == nodePos.y)
                continue;

            // Centro do segmento entre nodePos e aPos
            Vector mid;
            mid.x = (nodePos.x + aPos.x) / 2.0;
            mid.y = (nodePos.y + aPos.y) / 2.0;

            // Raio = metade da distância entre os nós
            double radius = CalculateDistance(nodePos, aPos) / 2.0;
            bool edgeValid = true;

            for (auto itB = m_table.begin(); itB != m_table.end(); ++itB)
            {
                const Ipv4Address& bId = itB->first;
                const Vector& bPos = itB->second.first;

                if (bId == aId)
                    continue;

                double distMidB = CalculateDistance(mid, bPos);

                // Se B estiver dentro do círculo de diâmetro (nodePos, aPos)
                if (distMidB < radius - 1e-6)
                {
                    edgeValid = false;
                    break;
                }
            }

            if (edgeValid)
                gabrielNeighbors.push_back(std::make_pair(aId, aPos));
        }

        return gabrielNeighbors;
    }



    std::vector<std::pair<Ipv4Address, Vector>>
    PositionTable::GetRngNeighbors(const Vector& nodePos)
    {
        Purge();
        std::vector<std::pair<Ipv4Address, Vector>> rngNeighbors;

        for (auto itA = m_table.begin(); itA != m_table.end(); ++itA)
        {
            const Ipv4Address& aId = itA->first;
            const Vector& aPos = itA->second.first;

            if (aPos.x == nodePos.x && aPos.y == nodePos.y)
                continue;

            double dNA = CalculateDistance(nodePos, aPos);
            bool edgeValid = true;

            for (auto itB = m_table.begin(); itB != m_table.end(); ++itB)
            {
                const Ipv4Address& bId = itB->first;
                const Vector& bPos = itB->second.first;

                if (bId == aId)
                    continue;

                double dNB = CalculateDistance(nodePos, bPos);
                double dAB = CalculateDistance(aPos, bPos);

                if (dAB < std::max(dNA, dNB) - 1e-6)
                {
                    edgeValid = false;
                    break;
                }
            }

            if (edgeValid)
                rngNeighbors.push_back(std::make_pair(aId, aPos));
        }

        return rngNeighbors;
    }


    /**
     * \brief Gets next hop according to GPSR recovery-mode protocol(right hand rule)
     * \param previousHop the position of the node that sent the packet to this node
     * \param nodePos the position of the destination node
     * \return Ipv4Address of the next hop, Ipv4Address::GetZero() if no nighbour was found in greedy mode
     */
    Ipv4Address
    PositionTable::BestAngle(Vector previousHop, Vector nodePos)
    {
        if (m_table.empty())
        {
            NS_LOG_DEBUG("BestNeighbor table is empty; Position: " << nodePos);
            return Ipv4Address::GetZero();
        }

        double tmpAngle;
        Ipv4Address bestFoundID = Ipv4Address::GetZero();
        double bestFoundAngle = 360;

        std::vector<std::pair<Ipv4Address, Vector>> neighbors;

        if (graphType == GPSR_NEIGHBOUR_TYPE_NONE)
            neighbors = GetNeighbors();
        
        else if (graphType == GPSR_NEIGHBOUR_TYPE_GABRIEL)
            neighbors = GetGabrielNeighbors(nodePos);
        
        else if (graphType == GPSR_NEIGHBOUR_TYPE_RNG)
            neighbors = GetRngNeighbors(nodePos);
        
        if (neighbors.empty())
        {
            NS_LOG_DEBUG("BestNeighbor table is empty; Position: " << nodePos);
            return Ipv4Address::GetZero();
        }

        for (auto i = neighbors.begin(); i != neighbors.end(); ++i)
        {
            tmpAngle = GetAngle(nodePos, previousHop, i->second);
            if (bestFoundAngle > tmpAngle)
            {
                bestFoundID = i->first;
                bestFoundAngle = tmpAngle;
            }
        }

        if (bestFoundID == Ipv4Address::GetZero())
            bestFoundID = neighbors.begin()->first;
        
        return bestFoundID;
    }

    //Gives angle between the vector CentrePos-Refpos to the vector CentrePos-node counterclockwise
    double PositionTable::GetAngle(Vector centrePos, Vector refPos, Vector node)
{
    double dx1 = node.x - centrePos.x;
    double dy1 = node.y - centrePos.y;
    double dx2 = refPos.x - centrePos.x;
    double dy2 = refPos.y - centrePos.y;

    double angle1 = atan2(dy1, dx1);
    double angle2 = atan2(dy2, dx2);

    double angle = angle1 - angle2;
    if (angle < 0) angle += 2*M_PI;

    return angle * 180.0 / M_PI;
}
    /**
     * \ProcessTxError
     */
    void PositionTable::ProcessTxError(WifiMacHeader const & hdr)
    {
    }

    //FIXME ainda preciso disto agr que o LS ja n está aqui???????

    /**
     * \brief Returns true if is in search for destionation
     */
    bool PositionTable::IsInSearch(Ipv4Address id)
    {
        return false;
    }

    bool PositionTable::HasPosition(Ipv4Address id)
    {
        return true;
    }
  }   // gpsr
} // ns3
