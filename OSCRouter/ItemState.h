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

#pragma once
#ifndef ITEM_STATE_H
#define ITEM_STATE_H

#include <vector>

class QColor;
class QString;

////////////////////////////////////////////////////////////////////////////////

class ItemState
{
public:
  enum EnumState
  {
    STATE_UNINITIALIZED = 0,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_NOT_CONNECTED,

    STATE_COUNT
  };

  ItemState()
    : state(STATE_UNINITIALIZED)
    , activity(false)
    , dirty(false)
  {
  }

  bool operator==(const ItemState &other) const;
  bool operator!=(const ItemState &other) const;

  EnumState state;
  bool activity;
  bool dirty;

  static void GetStateName(EnumState state, QString &name);
  static void GetStateColor(EnumState state, QColor &color);
};

////////////////////////////////////////////////////////////////////////////////

class ItemStateTable
{
public:
  typedef size_t ID;
  typedef std::vector<ItemState> LIST;

  ItemStateTable();

  virtual void Clear();
  virtual void Reset();
  virtual void Deactivate();
  virtual void Flush(ItemStateTable &other);
  virtual bool GetDirty() const { return m_Dirty; }
  virtual ID Register();
  virtual void Update(ID id, const ItemState &state);
  virtual const ItemState *GetItemState(ID id) const;
  virtual const LIST &GetList() const { return m_List; }

  static const ID sm_Invalid_Id;

private:
  bool m_Dirty;
  LIST m_List;
};

////////////////////////////////////////////////////////////////////////////////

#endif
