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

////////////////////////////////////////////////////////////////////////////////

const ItemStateTable::ID ItemStateTable::sm_Invalid_Id = static_cast<ItemStateTable::ID>(0xffffffff);

////////////////////////////////////////////////////////////////////////////////

bool ItemState::operator==(const ItemState &other) const
{
  return (state == other.state && activity == other.activity);
}

////////////////////////////////////////////////////////////////////////////////

bool ItemState::operator!=(const ItemState &other) const
{
  return (state != other.state || activity != other.activity);
}

////////////////////////////////////////////////////////////////////////////////

void ItemState::GetStateName(EnumState state, QString &name)
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

////////////////////////////////////////////////////////////////////////////////

void ItemState::GetStateColor(EnumState state, QColor &color)
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

ItemStateTable::ItemStateTable()
  : m_Dirty(false)
{
}

////////////////////////////////////////////////////////////////////////////////

void ItemStateTable::Clear()
{
  m_List.clear();
  m_Dirty = false;
}

////////////////////////////////////////////////////////////////////////////////

void ItemStateTable::Reset()
{
  for (LIST::iterator i = m_List.begin(); i != m_List.end(); i++)
    i->activity = i->dirty = false;
  m_Dirty = false;
}

////////////////////////////////////////////////////////////////////////////////

void ItemStateTable::Deactivate()
{
  ItemState deactivated;
  for (ID i = 0; i < m_List.size(); i++)
    Update(i, deactivated);
}

////////////////////////////////////////////////////////////////////////////////

void ItemStateTable::Flush(ItemStateTable &other)
{
  if (other.m_Dirty)
  {
    for (ID i = 0; i < other.m_List.size(); i++)
    {
      ItemState &otherItemState = other.m_List[i];
      Update(i, otherItemState);
      otherItemState.dirty = false;
      otherItemState.activity = false;
    }

    other.m_Dirty = false;
  }
}

////////////////////////////////////////////////////////////////////////////////

ItemStateTable::ID ItemStateTable::Register()
{
  m_List.push_back(ItemState());
  return (m_List.size() - 1);
}

////////////////////////////////////////////////////////////////////////////////

void ItemStateTable::Update(ID id, const ItemState &state)
{
  if (id < m_List.size())
  {
    ItemState &itemState = m_List[id];
    if (itemState != state)
    {
      itemState = state;
      itemState.dirty = true;
      m_Dirty = true;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

const ItemState *ItemStateTable::GetItemState(ID id) const
{
  return ((id < m_List.size()) ? (&(m_List[id])) : 0);
}

////////////////////////////////////////////////////////////////////////////////
