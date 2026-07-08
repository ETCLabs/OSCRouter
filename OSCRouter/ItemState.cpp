// Copyright (c) 2018 Electronic Theatre Controls, Inc., http://www.etcconnect.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "ItemState.h"
#include "QtInclude.h"

// must be last include
#include "LeakWatcher.h"

const ItemStateTable::ID ItemStateTable::sm_Invalid_Id = static_cast<ItemStateTable::ID>(0xffffffff);

////////////////////////////////////////////////////////////////////////////////

void ItemState::GetStateName(EnumState state, QString& name)
{
  switch (state)
  {
    case STATE_UNINITIALIZED: name = qApp->tr("..."); return;
    case STATE_CONNECTING: name = qApp->tr("Connecting..."); return;
    case STATE_CONNECTED: name = qApp->tr("Running"); return;
    case STATE_NOT_CONNECTED: name = qApp->tr("Not Running"); return;
  }

  name = QString();
}

void ItemState::GetStateColor(EnumState state, QColor& color)
{
  switch (state)
  {
    case STATE_CONNECTING: color = WARNING_COLOR; return;
    case STATE_CONNECTED: color = SUCCESS_COLOR; return;
    case STATE_NOT_CONNECTED: color = ERROR_COLOR; return;
  }

  color = MUTED_COLOR;
}

////////////////////////////////////////////////////////////////////////////////

void ItemStateTable::Clear()
{
  m_List.clear();
  m_Dirty = m_MuteDirty = false;
}

void ItemStateTable::Reset()
{
  for (LIST::iterator i = m_List.begin(); i != m_List.end(); i++)
    i->activity = i->dirty = false;
  m_Dirty = m_MuteDirty = false;
}

void ItemStateTable::Deactivate()
{
  ItemState deactivated;
  for (ID i = 0; i < m_List.size(); i++)
    Update(i, deactivated);
}

void ItemStateTable::Sync(ItemStateTable& other)
{
  size_t count = qMin(m_List.size(), other.m_List.size());

  // update UI from Router Thread
  if (other.m_Dirty)
  {
    for (size_t i = 0; i < count; i++)
    {
      ItemState& otherItemState = other.m_List[i];
      Update(i, otherItemState);
      otherItemState.dirty = false;
      otherItemState.activity = false;
    }

    other.m_Dirty = false;
  }

  // update Router Thread from UI
  other.m_MuteAllIncoming = m_MuteAllIncoming;
  other.m_MuteAllOutgoing = m_MuteAllOutgoing;

  if (m_MuteDirty)
  {
    for (size_t i = 0; i < count; i++)
      other.m_List[i].mute = m_List[i].mute;

    m_MuteDirty = false;
  }
}

ItemStateTable::ID ItemStateTable::Register(bool mute)
{
  ItemState state;
  state.mute = mute;
  m_List.push_back(state);
  return (m_List.size() - 1);
}

void ItemStateTable::Update(ID id, const ItemState& other)
{
  if (id >= m_List.size())
    return;

  ItemState& itemState = m_List[id];
  if (itemState.state == other.state && itemState.activity == other.activity)
    return;

  itemState.state = other.state;
  itemState.activity = other.activity;
  itemState.dirty = true;
  m_Dirty = true;
}

void ItemStateTable::Mute(ID id, bool b)
{
  if (id >= m_List.size())
    return;

  ItemState& itemState = m_List[id];
  if (itemState.mute == b)
    return;

  itemState.mute = b;
  m_MuteDirty = true;
}

const ItemState* ItemStateTable::GetItemState(ID id) const
{
  return ((id < m_List.size()) ? (&(m_List[id])) : 0);
}

////////////////////////////////////////////////////////////////////////////////
