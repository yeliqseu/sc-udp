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

#include "tcp-option-fec.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpOptionFEC");

NS_OBJECT_ENSURE_REGISTERED (TcpOptionFEC);

TcpOptionFEC::TcpOptionFEC ()
  : TcpOption (),
    m_flags (0),
    m_range (0)
{
}

TcpOptionFEC::~TcpOptionFEC ()
{
}

TypeId
TcpOptionFEC::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpOptionFEC")
    .SetParent<TcpOption> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpOptionFEC> ()
  ;
  return tid;
}

TypeId
TcpOptionFEC::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void
TcpOptionFEC::Print (std::ostream &os) const
{
  os << m_flags << ";" << m_range;
}

uint32_t
TcpOptionFEC::GetSerializedSize (void) const
{
  return 7;
}

void
TcpOptionFEC::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.WriteU8 (GetKind ()); // Kind
  i.WriteU8 (7); // Length
  i.WriteU8 (m_flags); // FEC flags
  i.WriteHtonU32 (m_range); // FEC range (ENCODED) or lost bytes (R_FAILED)
}

uint32_t
TcpOptionFEC::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  uint8_t readKind = i.ReadU8 ();
  if (readKind != GetKind ())
    {
      NS_LOG_WARN ("Malformed FEC option");
      return 0;
    }

  uint8_t size = i.ReadU8 ();
  if (size != 7)
    {
      NS_LOG_WARN ("Malformed FEC option");
      return 0;
    }
  m_flags = i.ReadU8 ();
  m_range = i.ReadNtohU32 ();
  return GetSerializedSize ();
}

uint8_t
TcpOptionFEC::GetKind (void) const
{
  return TcpOption::FEC;
}

void TcpOptionFEC::SetFlag (uint8_t flag) 
{
  m_flags = flag;
}

void TcpOptionFEC::SetRange (uint32_t range) 
{
  m_range = range;
}

uint8_t TcpOptionFEC::GetFlag (void) const
{
  return m_flags;
}

std::string TcpOptionFEC::GetFlagString (void) const
{
  std::string result;
  switch (m_flags) {
    case TcpOptionFEC::SYN:
      result = "SYN";
      break;
    case TcpOptionFEC::R_CWR:
      result = "R_CWR";
      break;
    case TcpOptionFEC::R_SUCCESS:
      result = "R_SUCCESS";
      break;
    case TcpOptionFEC::R_FAIL:
      result = "R_FAIL";
      break;
    case TcpOptionFEC::ENCODED:
      result = "ENCODED";
      break;
    case TcpOptionFEC::UNCODED:
      result = "UNCODED";
      break;
    default:
      result = "UNKNOWN";
  }
  return result;
}

uint32_t TcpOptionFEC::GetRange (void) const
{
  return m_range;
}

} // namespace ns3
