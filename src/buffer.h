#ifndef BUFFER_H
# define BUFFER_H

# include <algorithm>

# include <sys/uio.h>
# include <string.h>
# include <string>

# include "errors.h"
# include "base.h"
# include "macros.h"


// General design:
//   - I have a buffer of data. A buffer provides FIFO like features.
//     It has two cursors: one at the end of the FIFO, one at the
//     beginning of the FIFO.
//   - Reading from the beginning of the FIFO is called 'outputing' data.
//   - Appending at the end of the FIFO is called 'inputing' data.
//   - We thus have an OutputCursor, and an InputCursor.
//
// OutputCursor:
//   - constructor either takes a "buffer" (or something that represents
//     a buffer), or another OutputCursor.
//   - if an output cursor reads from the buffer data that is no longer
//     referenced in any other buffer, this data should be freed.
//   - if data is appended to a buffer through an input cursor, all
//     output cursors will see the new data.
//
// OutputCursor cursor(othercursor);
// OutputCursor cursor(chunkset);


class BufferChunk {
 public:
  static const int kRoundedSize = 1 << 14;

  explicit BufferChunk(int size)
      : data_(size ? new char[size] : NULL),
        size_(size),
        used_(0),
	absolute_offset_(0),
        refcount_(0),
        next_(NULL) {
  }
  ~BufferChunk() { delete []data_; }

  static int RoundSize(int size) {
    if (size < kRoundedSize)
      return kRoundedSize;
    return size;
  }

  // Simple accessors.
  unsigned int Size() const { return size_; }
  unsigned int Used() const { return used_; }
  unsigned int Available() const { return size_ - used_; }
  unsigned int AbsoluteOffset() const { return absolute_offset_; }

  char* Data() { return data_; }
  const char* Data() const { return data_; }

  // Modifiers.
  void IncrementUsed(int amount) {
    DEBUG_FATAL_UNLESS(used_ + amount <= size_)(
        "incrementing by more than the available amount? %d, %d, %d",
	used_, amount, size_);
    used_ += amount;
  }

  BufferChunk* Next() const { return next_; }

  void SetNext(BufferChunk* next) { next_ = next; }
  void SetAbsoluteOffset(unsigned int offset) { absolute_offset_ = offset; }
  void SetRefCount(int refcount) { refcount_ = refcount; }

  void Reference() { refcount_ += 1; }
  bool Unreference() { return --refcount_; }
  int RefCount() { return refcount_; }

 private:
  char* data_;
  // How much memory was allocated for the chunk?
  unsigned int size_;
  // How much memory has actually been used?
  unsigned int used_;
  // What is the offset of this chunk?
  unsigned int absolute_offset_;

  // How many cursors are referencing this buffer?
  int refcount_;

  BufferChunk* next_;
};

class ChunkSet {
 public:
  ChunkSet() : chunk_refcount_(0), placemark_(0),
      first_(&placemark_), last_(&placemark_) {
    placemark_.SetRefCount(1);
  }

  // Adds a new chunk to the chunkset.
  void Append(BufferChunk* chunk);
  // Remember that the next chunk after the one passed as argument
  // is to be referenced one less time. If it can be deleted, delete it.
  BufferChunk* Unreference(BufferChunk* chunk);

  // Decrement by one the refcount of the specified chunk,
  // and eventually delete it, and all the following chunks.
  void UnreferenceFrom(BufferChunk* previous);
  // The opposite of UnreferenceFrom.
  void ReferenceFrom(BufferChunk* previous);

  BufferChunk* First() const { return first_; }
  BufferChunk* Last() const { return last_; }

 private:
  int chunk_refcount_;
  BufferChunk placemark_;

  BufferChunk* first_;
  BufferChunk* last_;
};

class OutputCursor {
 public:
  static const int kIovecSize = 16;
  typedef struct iovec Iovec[kIovecSize];

  explicit OutputCursor(ChunkSet* data);
  explicit OutputCursor(const OutputCursor& cursor);
  ~OutputCursor();

  // Get a pointer to the data into the buffer. You can safely access
  // ContiguousSize() bytes from this pointer.
  const char* Data() const { return Pointer(); }
  char* Data() { return Pointer(); }
  // Consume some data from the current buffer. After calling Increment(),
  // Data() might return a different pointer and ContiguousSize() will return
  // a different size.
  void Increment(unsigned int amount);

  // Note that GetIovec returns pointers into the buffer, so data is not
  // consumed. You must call Increment() to remove data from buffer.
  unsigned int GetIovec(Iovec* vect, unsigned int* size) const;

  // Note that data read through Get.* methods is consumed, eg, buffer is
  // Increment()ed by the amount of data read.
  unsigned int Get(char* buffer, unsigned int size);
  void GetString(string* str);
  int GetString(string* str, unsigned int size);

  // How many bytes can be read in one go from Data() now?
  unsigned int ContiguousSize() const;
  // How many bytes in total are left to read?
  unsigned int LeftSize() const;
  void LimitLeftSize(unsigned int size);

  // Where are we at, when reading?
  BufferChunk* Chunk() const { return data_->First(); }
  int Offset() const { return offset_; }

 private:
  char* Pointer() const;

  BufferChunk* CurrentChunk() const;
  unsigned int CurrentChunkContiguousSize() const;
  void MoveToNextChunk();

  ChunkSet* data_;

  BufferChunk* previous_;

  // offset is zeroed every time we move to the next buffer. It's the
  // offset within the buffer.
  unsigned int offset_;

  // If set to true, it means that we ignore the real size of the buffer,
  // we just return limited_size_ instead.
  bool size_is_limited_;
  unsigned int limited_size_;
};

class InputCursor {
 public:
  explicit InputCursor(ChunkSet* chunk);

  const char* Data() const { return Pointer(); }
  char* Data() { return Pointer(); }

  // How many bytes can be copied into Data() in one go?
  unsigned int ContiguousSize() const;

  void Increment(unsigned int size);
  void Reserve(unsigned int size);
  void Add(const char* data, unsigned int size);

  void Add(const string& str) {
    Add(str.c_str(), str.size());
  }

 private:
  NO_COPY(InputCursor);

  char* Pointer() const;
  ChunkSet* data_;
};

class Buffer {
 public:
  Buffer() : input_cursor_(&data_), output_cursor_(&data_) {}

  InputCursor* Input() { return &input_cursor_; }
  OutputCursor* Output() { return &output_cursor_; }

  const InputCursor* Input() const { return &input_cursor_; }
  const OutputCursor* Output() const { return &output_cursor_; }

 private:
  ChunkSet data_;

  InputCursor input_cursor_;
  OutputCursor output_cursor_;
};

inline InputCursor::InputCursor(ChunkSet* data) : data_(data) {
}

inline unsigned int InputCursor::ContiguousSize() const {
  if (!data_->Last())
    return 0;
  return data_->Last()->Available();
}

inline char* InputCursor::Pointer() const {
  if (!data_->Last())
    return NULL;
  return data_->Last()->Data() + data_->Last()->Used();
}

inline void InputCursor::Increment(unsigned int size) {
  LOG_DEBUG("incrementing by %d", size);
  if (size) {
    DEBUG_FATAL_UNLESS(data_->Last() && size <= data_->Last()->Available())(
        "incrementing by more than the available amount? %p %d %d",
	(void *)data_->Last(), size, data_->Last()->Available());
    data_->Last()->IncrementUsed(size);
  }
}

inline void InputCursor::Reserve(unsigned int size) {
  if (!data_->Last() || data_->Last()->Available() < size)
    data_->Append(new BufferChunk(BufferChunk::RoundSize(size)));
}

inline void InputCursor::Add(const char* buffer, unsigned int size) {
  Reserve(size);
  while (1) {
    int tocopy = min(size, data_->Last()->Available());
    memcpy(Data(), buffer, tocopy);
    Increment(tocopy);

    size -= tocopy;
    if (!size)
      break;

    data_->Append(new BufferChunk(BufferChunk::RoundSize(size)));
  }
}

inline OutputCursor::OutputCursor(ChunkSet* data)
    : data_(data), previous_(data->First()), offset_(0),
      size_is_limited_(false) {
  data_->ReferenceFrom(previous_);
}

inline OutputCursor::OutputCursor(const OutputCursor& cursor)
    : data_(cursor.data_), previous_(cursor.previous_),
      offset_(cursor.offset_), size_is_limited_(cursor.size_is_limited_),
      limited_size_(cursor.limited_size_) {
  data_->ReferenceFrom(previous_);
}

inline OutputCursor::~OutputCursor() {
  data_->UnreferenceFrom(previous_);
}

inline BufferChunk* OutputCursor::CurrentChunk() const {
  return previous_->Next();
}

inline void OutputCursor::MoveToNextChunk() {
  previous_ = data_->Unreference(previous_);
  offset_ = 0;
}

inline unsigned int OutputCursor::ContiguousSize() const {
  unsigned int size(CurrentChunkContiguousSize());
  if (size || !CurrentChunk())
    return size;
  BufferChunk* chunk(CurrentChunk()->Next());
  if (chunk)
    return chunk->Used();
  return 0;
}

inline unsigned int OutputCursor::CurrentChunkContiguousSize() const {
  BufferChunk* chunk(CurrentChunk());
  if (!chunk)
    return 0;
  return chunk->Used() - offset_;
}

inline void OutputCursor::LimitLeftSize(unsigned int limited_size) {
  DEBUG_FATAL_UNLESS(limited_size <= LeftSize())(
      "increasing limited size past size of the buffer");

  size_is_limited_ = true;
  limited_size_ = limited_size;
}

inline unsigned int OutputCursor::LeftSize() const {
  BufferChunk* current(CurrentChunk());
  if (!current) {
    LOG_DEBUG("no current chunk");
    return 0;
  }

  BufferChunk* last(data_->Last());
//  LOG_DEBUG("last offset %d, current offset %d",
//	    last->AbsoluteOffset(), current->AbsoluteOffset());
  
  unsigned int real_size(
      (last->AbsoluteOffset() - current->AbsoluteOffset()) -
      offset_ + last->Used());

  if (size_is_limited_)
    return min(limited_size_, real_size);

  return real_size;
}

inline char* OutputCursor::Pointer() const {
  BufferChunk* chunk(CurrentChunk());
  if (!chunk)
    return NULL;
  if (chunk->Used() - offset_)
    return chunk->Data() + offset_;
  chunk = chunk->Next();
  if (chunk)
    return chunk->Data();
  return NULL;
}

inline void OutputCursor::Increment(unsigned int amount) {
  DEBUG_FATAL_UNLESS(amount <= LeftSize())(
      "incrementing by more than the available amount? %d %d",
      amount, LeftSize());
  LOG_DEBUG("incrementing by %d, contiguous size is %d, left size is %d",
	    amount, ContiguousSize(), LeftSize());
  do {
    if (CurrentChunkContiguousSize() >= amount) {
      offset_ += amount;
      break;
    }

    amount -= CurrentChunkContiguousSize();
    if (CurrentChunk()->Next())
      MoveToNextChunk();
  } while (amount && CurrentChunkContiguousSize());
}

inline unsigned int OutputCursor::Get(char* ptr, unsigned int size) {
  unsigned int tocopy;
  unsigned int left = size;
  while (left && LeftSize()) {
    tocopy = min(left, ContiguousSize());

    LOG_DEBUG("copying %d bytes (requested=%d, left=%d, contiguous=%d)",
	  tocopy, size, left, ContiguousSize());
    memcpy(ptr, Data(), tocopy);
    Increment(tocopy);

    left -= tocopy;
    ptr += tocopy;
  }

  return size - left;
}

inline unsigned int OutputCursor::GetIovec(
    OutputCursor::Iovec* vect, unsigned int* size) const {
  if (!LeftSize()) {
    *size = 0;
    return 0;
  }

  int count(0);
  int retval(0);
  BufferChunk* chunk(CurrentChunk());
  if (CurrentChunkContiguousSize()) {
    (*vect)[0].iov_base = const_cast<char*>(
        chunk->Data() + offset_);
    (*vect)[0].iov_len = CurrentChunkContiguousSize();
    retval += CurrentChunkContiguousSize();
    count = 1;
  }

  chunk = chunk->Next();
  for (; chunk && count < kIovecSize; ++count) {
    (*vect)[count].iov_base = chunk->Data(); 
    (*vect)[count].iov_len = chunk->Used();
    retval += chunk->Used();
    chunk = chunk->Next();
  } 

  *size = count;
  return retval;
}

inline void OutputCursor::GetString(string* str) {
  GetString(str, LeftSize());
}

inline int OutputCursor::GetString(string* str, unsigned int size) {
  str->reserve(size);

  unsigned int tocopy;
  unsigned int left = size;
  while (left && LeftSize()) {
    tocopy = min(left, ContiguousSize());
    str->append(Data(), tocopy);

    Increment(tocopy);
    left -= tocopy;
  }

  return left;
}

inline BufferChunk* ChunkSet::Unreference(BufferChunk* chunk) {
  if (chunk->Unreference()) {
    return chunk->Next();
  }

  LOG_DEBUG("chunk %08x is no longer used, first: %08x, last: %08x",
	    (unsigned int)chunk, (unsigned int)first_, (unsigned int)last_);

  DEBUG_FATAL_UNLESS(chunk != &placemark_)(
      "trying to delete placemark? you're sick, man.");
  DEBUG_FATAL_UNLESS(chunk == placemark_.Next())(
      "chunk != first is now unreferenced?? hole in a chunkset??");

  placemark_.SetNext(chunk->Next());
  if (last_ == chunk) {
    DEBUG_FATAL_UNLESS(placemark_.Next() == NULL)(
        "we just marked placemark as last chunk, but placemark_.Next() != NULL"
	", is there another chunk afterward?");
    last_ = &placemark_;
  }
  delete chunk;
  return placemark_.Next();
}

inline void ChunkSet::Append(BufferChunk* chunk) {
  if (last_) {
    last_->SetNext(chunk);
    chunk->SetAbsoluteOffset(
        last_->AbsoluteOffset() + last_->Used());
  } else {
    first_ = chunk;
  }
  chunk->SetRefCount(chunk_refcount_);
  last_ = chunk;
}

inline void ChunkSet::ReferenceFrom(BufferChunk* chunk) {
  chunk_refcount_ += 1;
  for (; chunk; chunk = chunk->Next())
    chunk->Reference();
}

inline void ChunkSet::UnreferenceFrom(BufferChunk* chunk) {
  chunk_refcount_ -= 1;
  while (chunk)
    chunk = Unreference(chunk);
}

#endif /* BUFFER_H */
