/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2024,  The University of Memphis,
 *                           Regents of the University of California,
 *                           Arizona Board of Regents.
 *
 * This file is part of NLSR (Named-data Link State Routing).
 * See AUTHORS.md for complete list of NLSR authors and contributors.
 *
 * NLSR is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NLSR is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NLSR, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "name-lsa.hpp"
#include "tlv-nlsr.hpp"

namespace nlsr {

NameLsa::NameLsa(const ndn::Name& originRouter, uint64_t seqNo,
                 const ndn::time::system_clock::time_point& timepoint,
                 const NamePrefixList& npl)
  : Lsa(originRouter, seqNo, timepoint)
{
  for (const auto& name : npl.getNameCosts()) {
    addName(name);
  }
}

NameLsa::NameLsa(const ndn::Block& block)
{
  wireDecode(block);
}

template<ndn::encoding::Tag TAG>
size_t
NameLsa::wireEncode(ndn::EncodingImpl<TAG>& block) const
{
  size_t totalLength = 0;

  auto names = m_npl.getNameCosts();

  for (auto it = names.rbegin();  it != names.rend(); ++it) {
    totalLength += it->wireEncode(block);
  }

  totalLength += Lsa::wireEncode(block);

  totalLength += block.prependVarNumber(totalLength);
  totalLength += block.prependVarNumber(nlsr::tlv::NameLsa);

  return totalLength;
}

NDN_CXX_DEFINE_WIRE_ENCODE_INSTANTIATIONS(NameLsa);

const ndn::Block&
NameLsa::wireEncode() const
{
  if (m_wire.hasWire()) {
    return m_wire;
  }

  ndn::EncodingEstimator estimator;
  size_t estimatedSize = wireEncode(estimator);

  ndn::EncodingBuffer buffer(estimatedSize, 0);
  wireEncode(buffer);

  m_wire = buffer.block();

  return m_wire;
}

void
NameLsa::wireDecode(const ndn::Block& wire)
{
  m_wire = wire;

  if (m_wire.type() != nlsr::tlv::NameLsa) {
    NDN_THROW(Error("NameLsa", m_wire.type()));
  }

  m_wire.parse();

  auto val = m_wire.elements_begin();

  if (val != m_wire.elements_end() && val->type() == nlsr::tlv::Lsa) {
    Lsa::wireDecode(*val);
    ++val;
  }
  else {
    NDN_THROW(Error("Missing required Lsa field"));
  }

  NamePrefixList npl;
  for (; val != m_wire.elements_end(); ++val) {
    if (val->type() == nlsr::tlv::PrefixCost) {
      //TODO: Implement this structure as a type instead and add decoding
      npl.insert(std::tuple<ndn::Name, size_t>(*val));
    }
    else {
      NDN_THROW(Error("Name", val->type()));
    }
  }
  m_npl = npl;
}

void
NameLsa::print(std::ostream& os) const
{
  os << "      Names:\n";
  int i = 0;
  for (const auto& name : m_npl.getNames()) {
    os << "        Name " << i++ << ": " << name << "\n";
  }
}

std::tuple<bool, std::list<std::tuple<ndn::Name, size_t>>, std::list<std::tuple<ndn::Name, size_t>>>
NameLsa::update(const std::shared_ptr<Lsa>& lsa)
{
  auto nlsa = std::static_pointer_cast<NameLsa>(lsa);
  bool updated = false;

  // Obtain the set difference of the current and the incoming
  // name prefix sets, and add those.
  std::list<std::tuple<ndn::Name, size_t>> newNames = nlsa->getNpl().getNameCosts();
  std::list<std::tuple<ndn::Name, size_t>> oldNames = m_npl.getNameCosts();
  std::list<std::tuple<ndn::Name, size_t>> namesToAdd;
  std::set_difference(newNames.begin(), newNames.end(), oldNames.begin(), oldNames.end(),
                      std::inserter(namesToAdd, namesToAdd.begin()));
  for (const auto& name : namesToAdd) {
    addName(name);
    updated = true;
  }

  // Also remove any names that are no longer being advertised.
  std::list<std::tuple<ndn::Name, size_t>> namesToRemove;
  std::set_difference(oldNames.begin(), oldNames.end(), newNames.begin(), newNames.end(),
                      std::inserter(namesToRemove, namesToRemove.begin()));
  for (const auto& name : namesToRemove) {
    removeName(name);
    updated = true;
  }

  return {updated, namesToAdd, namesToRemove};
}

} // namespace nlsr
