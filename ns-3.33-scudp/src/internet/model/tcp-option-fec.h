/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Adrian Sai-wah Tam
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
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
 * Author: Adrian Sai-wah Tam <adrian.sw.tam@gmail.com>
 */

#ifndef TCP_OPTION_FEC_H
#define TCP_OPTION_FEC_H

#include "ns3/tcp-option.h"

namespace ns3 {

/**
 * \ingroup tcp
 *
 * Defines the TCP option of kind X as described in TCP-IR (https://datatracker.ietf.org/doc/html/draft-flach-tcpm-fec-00)
 */

class TcpOptionFEC : public TcpOption
{
public:
  TcpOptionFEC ();
  virtual ~TcpOptionFEC ();

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  virtual void Print (std::ostream &os) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);

  virtual uint8_t GetKind (void) const;
  virtual uint32_t GetSerializedSize (void) const;

  // FEC flags
  typedef enum
  {
    SYN = 0,          // used for packets with SYN flag set
    R_CWR = 1,        // Congestion window reduction acknowledgement (not used in Ferlin2019TNET)
    R_SUCCESS  = 2,   // Recovery successful
    R_FAIL  = 4,      // Recovery failed
    ENCODED  = 8,     // Packet is encoded
    UNCODED  = 16,    // Packet is not coded
  } Flags_t;

  void SetFlag (uint8_t);

  void SetRange (uint32_t);

  uint8_t GetFlag (void) const;

  std::string GetFlagString (void) const;

  uint32_t GetRange (void) const;

protected:
  uint8_t    m_flags;
  uint32_t   m_range;
};

} // namespace ns3

#endif /* TCP_OPTION_FEC */
