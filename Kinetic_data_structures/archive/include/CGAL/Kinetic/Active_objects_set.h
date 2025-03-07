// Copyright (c) 2005  Stanford University (USA).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org); you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 3 of the License,
// or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
// SPDX-License-Identifier: LGPL-3.0+
// 
//
// Author(s)     : Daniel Russel <drussel@alumni.princeton.edu>

#ifndef CGAL_KINETIC_NOTIFYING_SET_BASE_3_H
#define CGAL_KINETIC_NOTIFYING_SET_BASE_3_H
#include <CGAL/Kinetic/basic.h>
#include <CGAL/Tools/Label.h>
#include <CGAL/Kinetic/Ref_counted.h>
#include <CGAL/Tools/Counter.h>
#include <iostream>
#include <CGAL/Kinetic/Multi_listener.h>
#include <set>
#include <map>
#include <vector>
#include <sstream>
#include <CGAL/boost/iterator/transform_iterator.hpp>
#include <utility>

namespace CGAL { namespace Kinetic {;

//! Holds a set of moving points and creates notifications when changes occur.
/*!  This container holds a set of objects of a particular type. It
  creates notifications using the standard CGAL::Multi_listener
  interface when a primitive changes or is added or deleted. Objects
  which are listening for events can then ask which primitives
  changed.

  There is one type of notification
  Moving_object_table::Listener_core::IS_EDITING which occurs when the
  editing mode is set to false.

  As an optimization, the change methods can be called without setting
  the editing state to true, this acts as if it were set to true for
  that one function call.

  Objects are stored in a vector. This means that access is constant
  time, but storage is not generally freed. The only way to be sure is
  to remove all reference counts for the table or to call clear().
*/
template <class Value_t >
class Active_objects_set:
  public Ref_counted<Active_objects_set<Value_t > >
{
protected:
  //! Convenience
  typedef Active_objects_set<Value_t> This;
  typedef std::map<int, Value_t> Storage;
  struct Listener_core
  {
    typedef enum {IS_EDITING}
      Notification_type;
    typedef typename This::Handle Notifier_handle;
  };
public:
  //! How to refer to points
  typedef typename CGAL::Label<Value_t> Key;
  //! the type of the objects in the table
  typedef Value_t Data;

  //! The interface to implement if you want to receive notifications.
  typedef Multi_listener<Listener_core> Listener;

protected:
  // This is evil here.
  typedef std::set<Listener*> Subscribers;
public:

  //! default constructor
  Active_objects_set():editing_(false), next_key_(0){}

  ~Active_objects_set(){CGAL_precondition(subscribers_.empty());}

  //! access a point
  const Data &operator[](Key key) const
  {
    CGAL_expensive_precondition(key.is_valid());
    CGAL_precondition(storage_.find(key.index()) != storage_.end());
    //if (static_cast<unsigned int>(key.index()) >= storage_.size()) return null_object();
    /*else*/
    return storage_.find(key.index())->second;
  }

  //! non operator based method to access a point.
  const Data& at(Key key) const
  {
    return operator[](key);
  }

  //! Set the editing state of the object.
  /*!  A notification is sent when the editing state is set to false
    after it has been true, i.e. the editing session is finished. This
    allows changes to be batched together.
  */
  void set_is_editing(bool is_e) {
    if (is_e== editing_) return;
    editing_=is_e;
    if (!editing_) {
      finish_editing();
    }
  }

  //! return the editing state
  bool is_editing() const
  {
    return editing_;
  }

  //! change one point.
  /*!  The position at the current time should not be different from
    the previous current position. However, at the moment I do not
    check this as there is no reference to time in the
    Data_table.

    If Data_table::editing() is not true, then it is as if the calls
    - set_editing(true)
    - set_object(key, value)
    - set_editing(false)
    Were made. If it is true, then no notifications are created.

    \todo check continuity.
  */
  void set(Key key, const Data &new_value) {
    //CGAL_precondition(editing_);
    CGAL_precondition(key.is_valid());
    //CGAL_precondition(static_cast<unsigned int>(key.index()) < storage_.size());
    storage_[key.index()]=new_value;
    changed_objects_.push_back(key);
    if (!editing_) finish_editing();
  }

  //! Insert a new object into the table.
  /*!
    See Data_table::set_object() for an explainating of how the editing modes are used.
  */
  Key insert(const Data &ob) {
    //CGAL_precondition(editing_);
    Key ret(next_key_); ++next_key_;
    new_objects_.push_back(ret);
    storage_[ret.index()]=ob;

    if (!editing_) finish_editing();
    return ret;
  }

  //! Delete an object from the table.
  /*!
    The object with Key key must already be in the table.

    This does not necessarily decrease the amount of storage used at
    all. In fact, it is unlikely to do so.

    See Data_table::set_object() for an explainating of how the editing modes are used.
  */
  void erase(Key key) {
    //CGAL_precondition(editing_);
    CGAL_precondition(key.is_valid());
    CGAL_precondition(static_cast<unsigned int>(key.index()) < storage_.size());
    CGAL_expensive_precondition_code(for (Inserted_iterator dit= inserted_begin(); dit != inserted_end(); ++dit))
      {CGAL_expensive_precondition(*dit != key);}
    CGAL_expensive_precondition_code(for (Changed_iterator dit= changed_begin(); dit != changed_end(); ++dit))
      {CGAL_expensive_precondition(*dit != key);}
    deleted_objects_.push_back(key);
    if (!editing_) finish_editing();
  }

  //! Clear all points.
  void clear() {
    deleted_objects_.insert(deleted_objects_.end(), keys_begin(), keys_end());
    //storage_.clear();
    if (!editing_) finish_editing();
  }

  struct Get_key {
    typedef Key result_type;
    typedef typename Storage::value_type argument_type;
    Key operator()(const typename Storage::value_type k) const {
      return Key(k.first);
    }
  };

  //! An iterator to iterate through all the keys
  typedef typename boost::transform_iterator<Get_key,
					     typename Storage::const_iterator> Key_iterator;

  //! Begin iterating through the keys
  Key_iterator keys_begin() const
  {
    return Key_iterator(storage_.begin());
  }
  //! End iterating through the keys.
  Key_iterator keys_end() const
  {
    return Key_iterator(storage_.end());
  }

  //! An iterator for iterating through changed objects.
  /*!  The list of objects accessed through this iterator is only
    those changed in the last session.
  */
  typedef typename std::vector<Key>::const_iterator Changed_iterator;
  Changed_iterator changed_begin() const
  {
    return changed_objects_.begin();
  }
  Changed_iterator changed_end() const
  {
    return changed_objects_.end();
  }
  //! An iterator for iterating through added objects.
  /*!  The list of objects accessed through this iterator is only
    those added in the last session.
  */
  typedef typename std::vector<Key>::const_iterator Inserted_iterator;
  //! The begin iterator for new objects
  Inserted_iterator inserted_begin() const
  {
    return new_objects_.begin();
  }
  //! The past-end iterator for new objects.
  Inserted_iterator inserted_end() const
  {
    return new_objects_.end();
  }
  //! An iterator for iterating through deleted objects.
  /*!  The list of objects accessed through this iterator is only
    those deleted in the last session.
  */
  typedef typename std::vector<Key>::const_iterator Erased_iterator;
  //! The begin iterator for deleted objects.
  Erased_iterator erased_begin() const
  {
    return deleted_objects_.begin();
  }
  //! The past end iterator for deleted objects.
  Erased_iterator erased_end() const
  {
    return deleted_objects_.end();
  }
  //! The number of objects in the table
  /*!  The objects are stored in a vector, so this could be large than
    (additions- deletions) as non-consecutive blank objects not at the
    back are still counted. In general, deleting an object doesn't do
    much for storage, just provides notifications.
  */
  unsigned int size() const
  {
    return storage_.size();
  }

  std::ostream &write(std::ostream &out) const {
    for (unsigned int i=0; i< storage_.size(); ++i){
      out << storage_[i] << std::endl;
    }
    return out;
  }
  
  std::istream &read(std::istream &in) {
    if (!storage_.empty()) {
      set_is_editing(true);
      for (Key_iterator kit= keys_begin(); kit != keys_end(); ++kit){
	erase(*kit);
      }
      set_is_editing(false);
      storage_.clear();
    }
    set_is_editing(true);
    do {
      char buf[10000];
      in.getline(buf, 10000);
      if (!in || buf[0]=='\0' || buf[0]=='#') break;
      std::istringstream iss(buf);
      Data d; 
      iss >> d;
      if (!iss) {
	std::cerr << "ERROR reading object from line " << buf << std::endl;
	internal::fail__=true;
      } else {
	insert(d);
      }
    } while (true);
    set_is_editing(false);
    return in;
  }

private:
  friend class Multi_listener<Listener_core>;
  //! listen for changes
  /*!
    This method alerts the subscribe to all exising objects.
  */
  void new_listener(Listener *sub) const
  {
    subscribers_.insert(sub);
  }

  //! end listening for changes
  void delete_listener(Listener *sub) const
  {
    subscribers_.erase(sub);
  }

  void finish_editing() {
    for (typename Subscribers::iterator it= subscribers_.begin(); it != subscribers_.end(); ++it) {
      (*it)->new_notification(Listener::IS_EDITING);
    }

    for (Erased_iterator it= erased_begin(); it != erased_end(); ++it) {
      storage_.erase(it-.index());
    }

    changed_objects_.clear();
    deleted_objects_.clear();
    new_objects_.clear();
  }


protected:

  Storage storage_;
  mutable Subscribers subscribers_;
  std::vector<Key> changed_objects_;
  std::vector<Key> deleted_objects_;
  std::vector<Key> new_objects_;
  bool editing_;
  unsigned int next_key_;
  /*static const Data &null_object() {
    static Data o;
    return o;
    }*/
};

template <class V >
inline std::ostream &operator<<(std::ostream &out, const Active_objects_set<V> &v) {
  return v.write(out);
}


template <class V >
inline std::istream &operator>>(std::istream &in, Active_objects_set<V> &v) {
  return v.read(in);
}

} } //namespace CGAL::Kinetic;
#endif
