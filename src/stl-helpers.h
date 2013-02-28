#ifndef STL_HELPERS_H
# define STL_HELPERS_H

class Holder {
 public:
  virtual ~Holder() {}
  virtual void Delete() = 0;
};

template<typename TYPE>
class TypedHolder : public Holder {
 public:
  TypedHolder(TYPE* ptr) : type_(ptr) {}
  void Delete() { delete type_; }

 private:
  TYPE* type_;
};

template<typename MAP>
bool StlMapHas(const MAP& map, const typename MAP::key_type& key) {
  typename MAP::const_iterator it(map.find(key));
  if (it == map.end())
    return false;
  return true;
}

template<typename MAP>
typename MAP::value_type::second_type
StlMapGet(const MAP& map, const typename MAP::key_type& key) {
  typename MAP::const_iterator it(map.find(key));
  if (it == map.end())
    return NULL;
  return it->second;
}

template<typename HOLDER>
inline void StlDeleteElements(HOLDER* holder) {
  for (typename HOLDER::iterator it(holder->begin());
       it != holder->end(); ++it) {
    delete *it;
  }
}

template<typename HOLDER>
class StlAutoDeleteElements {
 public:
  explicit StlAutoDeleteElements(HOLDER* holder)
     : holder_(holder) {
  }

  ~StlAutoDeleteElements() {
    StlDeleteElements(holder_);
  }

 private:
  NO_COPY(StlAutoDeleteElements);

  HOLDER* holder_;
};

// FIXME: this is ugly.
#ifdef __GXX_EXPERIMENTAL_CXX0X__
# define AUTO_DELETE_ELEMENTS(holder) StlAutoDeleteElements<decltype(holder)> __auto_delete_##holder(&(holder))
#else
# define AUTO_DELETE_ELEMENTS(holder) StlAutoDeleteElements<typeof(holder)> __auto_delete_##holder(&(holder))
#endif

#endif /* STL_HELPERS_H */
