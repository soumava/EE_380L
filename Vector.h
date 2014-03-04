#ifndef _VECTOR_H_
#define _VECTOR_H_

using namespace std;
//
// Utility gives sdt::rel_ops which will fill in relational
// iterator operations so long as you provide the
// operators discussed in class.  In any case, ensure that
// all operations listed in this website are legal for your
// iterators:
// http://www.cplusplus.com/reference/iterator/RandomAccessIterator/
//

//using namespace std::rel_ops;

namespace epl
{
    class invalid_iterator {
    public:
        typedef enum { 
            SEVERE, 
            MODERATE, 
            MILD, 
            WARNING 
        } Severity_level;
        Severity_level level;

        invalid_iterator() { 
            level = SEVERE; 
        }
        
        invalid_iterator(Severity_level _level) { 
            level = _level; 
        }
        
        virtual const char* what() const throw() {
            switch (level){
            case WARNING:   return "Warning";
            case MILD:      return "Mild";
            case MODERATE:  return "Moderate";
            case SEVERE:    return "Severe";
            default:        return "ERROR";
            }
        }
    };

    template <typename It>
    struct iterator_traits {
        using value_type = typename It::value_type;
        using iterator_category = typename It::iterator_category;
    };
    
    template <typename T>
    class Vector {
    public:
        struct CtrlBlk {
        public:
            typedef enum {
                PUSH_BACK,
                POP_BACK,
                PUSH_FRONT,
                POP_FRONT,
                EMPLACE_BACK,
                COPY_ASSIGN,
                MOVE_ASSIGN,
                DESTROY,
                NONE
            } invalidate_reason;

            uint64_t _version;
            uint64_t _refs;
            invalidate_reason _reason;
            T *_location, *_begin, *_end;

            CtrlBlk() = delete;
            
            explicit CtrlBlk(int64_t version) {
                //
                // The CtrlkBlk object can only be created with an integer version
                // The default constructor has been deleted so that it cannot be 
                // called ainwayi
                //
                this->_version = version;
                this->_refs = 0;
                this->_reason = NONE;
                this->_location = nullptr;
                this->_begin = nullptr;
                this->_end = nullptr;
            }

            ~CtrlBlk() {}

            void incRef() {
                this->_refs++;
            }

            void decRef() {
                this->_refs--;
            }

            void invalidate(invalidate_reason reason, T* location, T* begin, T*end) {
                //
                // Invalidating the CtrlBlk - storing the reason for invalidation 
                // and also pertinent details about why it was invalidated. These details
                // help while throwing exceptions when the invalid iterator is used later.
                // 
                this->_version = INT_MIN;
                this->_reason = reason;
                this->_location = location;
                this->_begin = begin;
                this->_end = end;
            }
        };
        
        struct const_iterator {
        protected :
            T *_ptr, *_begin, *_end;
            CtrlBlk* _ctrlBlk;
            
            void validate_base() const {
                //
                // This method validates that the version of the vector is still okay
                // to use. If not, then it throws an exception: invalid_iterator
                //
                if (this->_ctrlBlk->_version == INT_MIN) {
                    //
                    // Iterator is invalid - check the various cases
                    // This function is called on the use of the value of the iterator
                    // Dereferencing uses a separate function
                    //
                    if (this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::DESTROY) {
                        //
                        // Iterator's internal state has been deleted
                        //
                        throw invalid_iterator(invalid_iterator::Severity_level::SEVERE);
                    }
                    else if (this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::COPY_ASSIGN ||
                        this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::MOVE_ASSIGN) {
                        //
                        // Iterator's internal state has been assigned
                        //
                        throw invalid_iterator(invalid_iterator::Severity_level::MODERATE);
                    }
                    else if ((this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::POP_BACK ||
                        this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::POP_FRONT) &&
                        (_ptr == this->_ctrlBlk->_location)) {
                        //
                        // Iterator has been reallocated, but its not a destroy or an assignment
                        // This is the case when a push_back, push_front, pop_back or pop_front reallocates
                        //
                        throw invalid_iterator(invalid_iterator::Severity_level::SEVERE);
                    }
                    else if ((this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::PUSH_BACK || 
                        this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::POP_BACK || 
                        this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::EMPLACE_BACK || 
                        this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::PUSH_FRONT || 
                        this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::POP_FRONT) && 
                        (_ptr < this->_ctrlBlk->_begin || _ptr > this->_ctrlBlk->_end)) {
                        //
                        // Iterator has been reallocated, but its not a destroy or an assignment
                        // This is the case when a push_back, push_front, pop_back or pop_front reallocates
                        //
                        throw invalid_iterator(invalid_iterator::Severity_level::MODERATE);
                    }
                    else if (this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::POP_FRONT ||
                        this->_ctrlBlk->_reason == CtrlBlk::invalidate_reason::PUSH_FRONT) {
                        //
                        // No change in the underlying system, just a push_front or a pop_front
                        // operation that might have changed the index
                        //
                        throw invalid_iterator(invalid_iterator::Severity_level::WARNING);
                    }
                    else {
                        throw invalid_iterator(invalid_iterator::Severity_level::MILD);
                    }
                }
            }

            void validate_deref() const {
                //
                // This method validates the cases where an iterator is dereferenced. 
                // In case it is done out of bounds, an out_of_range exception is thrown
                // In case the iterator is invalid, the more severe of the two exceptions is thrown
                //
                bool isOutOfRange = false;
                if (this->_ptr == nullptr || this->_ptr < this->_begin || this->_ptr >= this->_end) {
                    isOutOfRange = true;
                }
                //
                // Call validate_base inside a try-catch block
                // This way we can prioritize which exception to throw
                //
                try {
                    validate_base();
                }
                catch (epl::invalid_iterator& ex) {
                    //
                    // This exception to be thrown in 2 cases:
                    // i.  dereference is not out of range
                    // ii. exception is SEVERE
                    //
                    if (!isOutOfRange || ex.level == invalid_iterator::SEVERE) {
                        throw ex;
                    }
                }
                //
                // If the code is here, validate_base did not throw an exception
                // or it threw an exception that was suppressed in favor of out_of_range
                //
                if (isOutOfRange) {
                    throw std::out_of_range("Dereferencing pointer out of valid range.");
                }
            }

        public :
            const_iterator() : _ctrlBlk(nullptr), _ptr(nullptr), _begin(nullptr), _end(nullptr) {}

            const_iterator(const const_iterator& other) {
                this->_ctrlBlk = other._ctrlBlk;
                this->_ptr = other._ptr;
                this->_begin = other._begin;
                this->_end = other._end;
                this->_ctrlBlk->incRef();
            }

            const_iterator(T* ptr, T* begin, T* end, CtrlBlk* ctrlBlk) {
                this->_ptr = ptr;
                this->_begin = begin;
                this->_end = end;
                this->_ctrlBlk = ctrlBlk;
                this->_ctrlBlk->incRef();
            }

            using value_type = T;
            using iterator_category = std::random_access_iterator_tag;
            using reference = T&;
            using pointer = T*;
            using difference_type = uint64_t;

            const T& operator*(void) const {
                validate_deref();
                return const_cast<const T&>(*_ptr);
            }

            bool operator==(const_iterator& rhs) const {
                validate_base();
                return _ptr == rhs._ptr;
            }

            bool operator!=(const_iterator& rhs) const {
                validate_base();
                return !(*this == rhs);
            }

            int64_t operator-(const_iterator& rhs) const {
                validate_base();
                return (_ptr - rhs._ptr);
            }

            const_iterator operator+(int64_t offset) const {
                validate_base();
                const_iterator t{ *this };
                t._ptr = t._ptr + offset;
                return t;
            }

            const_iterator& operator++(void) {
                validate_base();
                _ptr = _ptr + 1;
                return *this;
            }

            const_iterator& operator--(void) {
                validate_base();
                _ptr = _ptr - 1;
                return *this;
            }

            ~const_iterator() {
                //
                // The destructor of the const_iterator object
                // It decrements the control block, and destructs it only if
                // the version is invalid and the refcount is 0 - this means that neither
                // the vector nor any other iterator is using this block.
                //
                this->_ctrlBlk->decRef();
                if (this->_ctrlBlk->_version == INT_MIN &&
                    this->_ctrlBlk->_refs == 0) {
                    delete(this->_ctrlBlk);
                }
            }
        };

        struct iterator : public const_iterator {
        public:
            iterator() = delete;
            
            iterator(T* ptr, T* begin, T* end, CtrlBlk* ctrlBlk) : const_iterator(ptr, begin, end, ctrlBlk) {}

            iterator(const iterator& other) : const_iterator(other) {};

            T& operator*(void) {
                validate_deref();
                return *_ptr;
            }

            iterator& operator++(void) {
                validate_base();
                _ptr = _ptr + 1;
                return *this;
            }

            iterator& operator--(void) {
                validate_base();
                _ptr = _ptr - 1;
                return *this;
            }
            
            operator const_iterator() {
                return *(static_cast<const_iterator*>(this));
            } 

            ~iterator() {
            }
            

//            iterator& operator=(iterator& rhs)
//            {
//                _ptr = rhs._ptr;
//                return *this;
//            }
        };
        
        Vector(void) {
            //
            // Creates an array with a minimum capacity of 8, and length equal to 0
            // Must not call T::T(void).
            //
            init(0);
#ifdef _DBG_
            cout << "epl::Vector::Default constructor of Vector. Created vector of size: 8" << endl;
#endif
        }

        explicit Vector(uint64_t n) {
            //
            // Create an array with capacity and length exactly equal to 'n'. 
            // Initialize the 'n' objects using T::T(void). As a special case, if 'n' is 
            // equal to zero, duplicates the behavior of Vector(void)
            //
            init(n);
#ifdef _DBG_
            cout << "epl::Vector::Explicit constructor of Vector. Created vector of size: " << n << endl;
#endif
        }

        Vector(const Vector& other) {
            //
            // Copy constructor - calls the private copy method
            //
#ifdef _DBG_
            cout << "epl::Vector::Copy constructor called." << endl;
#endif 
            copy(other);
        }

        Vector(Vector&& other) {
            //
            // Move constructor - shallow copying the data from the Vector on the 
            // rhs and setting all the state on the rhs to such values that will make its 
            // destruction a no-op.
            //
#ifdef _DBG_
            cout << "epl::Vector::Move constructor called." << endl;
#endif
            this->_buffer = other._buffer;
            this->_buffer_end = other._buffer_end;
            this->_front = other._front;
            this->_back = other._back;
            this->_length = other._length;
            this->_ctrlBlk = other._ctrlBlk;
            other._buffer = nullptr;
            other._buffer_end = nullptr;
            other._front = nullptr;
            other._back = nullptr;
            other._length = 0;
            other._ctrlBlk = nullptr;
        }

        Vector(std::initializer_list<T> init_list) {
            //
            // A constructor for the vector with a std::initializer_list as the argument
            // The constructor iterates over the contents of the list and calls 
            // the emplace_back method of the vector which takes care of amortized doubling
            //
            alloc(init_list.end() - init_list.begin());
            this->_ctrlBlk = new CtrlBlk(1);
            for (auto iter = init_list.begin(); iter != init_list.end(); iter++) {
                this->emplace_back(*iter);
            }
        }
 
        Vector& operator=(const Vector& other) {
            //
            // Copy assignment operator
            //
#ifdef _DBG_
            cout << "epl::Vector::Copy assignment operator called." << endl;
#endif
            if (this != &other) {
                destroy(CtrlBlk::COPY_ASSIGN);
                copy(other);
            }
            return *this;
        }

        Vector& operator=(Vector&& other) {
            //
            // Move assignment operator - swapping state with the rhs
            // When the rhs gets cleaned, the earlier state of this object will be cleaned
            //
#ifdef _DBG_
            cout << "epl::Vector::Move assignment operator called." << endl;
#endif
            std::swap(_buffer, other._buffer);
            std::swap(_front, other._front);
            std::swap(_back, other._back);
            std::swap(_buffer_end, other._buffer_end);
            std::swap(_length, other._length);
            std::swap(_ctrlBlk, other._ctrlBlk);
            other.update_ctrlBlk(CtrlBlk::MOVE_ASSIGN);
        }

        ~Vector(void) {
            // 
            // Destructor - simply calls the private destruction method
            //
#ifdef _DBG_
            cout << "epl::Vector::Destructor called." << endl;
#endif
            destroy(CtrlBlk::DESTROY);
        }

        uint64_t size(void) const {
            //
            // Method to return the number of elements in the Vector
            // Follows the below illustrated scheme
            // -----------------------------------------
            // |   | o | o | o | o | o |   |   |   |   |
            // -----------------------------------------
            //   0   1   2   3   4   5   6   7   8   9
            //       ^                   ^
            //     _front              _back
            // size = _back - _front = 6 - 1 = 5
            //
            // assert(length == (_back - _front));
#ifdef _DBG_
            cout << "epl::Vector::size called. Returning: " << _length << endl;
#endif
            return _length;
        }

        T& operator[](uint64_t k) {
            //
            // Array indexing operator. If 'k' is out of bounds (equal to or larger than length)
            // then std::out_of_range is thrown. If 'k' is in bounds, then a reference to the 
            // element at position 'k' is returned.
            //
#ifdef _DBG_
            cout << "epl::Vector::operator[] called for index " << k << endl;
#endif
            if (k < 0 || k >= _length) {
                throw std::out_of_range("Array Index out of Range.");
            }
            else {
                return _front[k];
            }
        }
        const T& operator[](uint64_t k) const {
            //
            // Same as above, except for constants.
            //
#ifdef _DBG_
            cout << "epl::Vector::operator[] const called for index " << k << endl;
#endif
            if (k < 0 || k >= _length) {
                throw std::out_of_range("Array Index out of Range.");
            }
            else {
                return const_cast<const T&>(_front[k]);
            }
        }
        
        iterator begin() {
            //
            // Returns an iterator to the _front of the vector
            //
            return iterator(_front, _front, _back, _ctrlBlk);
        }

        const_iterator begin() const {
            //
            // Returns a const iterator to the _front of the vector
            //
            return const_iterator(_front, _front, _back, _ctrlBlk);
        }

        iterator end() {
            //
            // Returns an iterator to the _back of the vector
            //
            return iterator(_back, _front, _back, _ctrlBlk);
        }

        const_iterator end() const {
            //
            // Returns a const iterator to the _back of the vector
            //
            return const_iterator(_back, _front, _back, _ctrlBlk);
        }

        template <typename... Args>
        void emplace_back(Args&&... args) {
            //
            // This is a variadic member template function that constructs the objects in place
            // Instead of constructing an object and then calling push_back on it (which makes 2 
            // constructor calls) this method directly calls the constructor by forwarding the
            // arguments to the constructor in place.
            //
            T* new_ptr = _buffer, *old_front = _front, *old_back = _back;
            uint64_t current_buffer_size = (_buffer_end - _buffer), offset = 0, old_buffer_size, old_length = _length;
            old_buffer_size = current_buffer_size;

            if (_back == _buffer_end) {
                //
                // The next point to insert the object is past the last cell
                // Amortized doubling of the array is in order
                // In this case _front and _back do not change, as all new memory is to the end
                //
                current_buffer_size = current_buffer_size * 2;
                new_ptr = (T *) operator new ((size_t)current_buffer_size * sizeof(T));
                _front = new_ptr + (_front - _buffer);
                _back = new_ptr + (_back - _buffer);
                _buffer_end = new_ptr + current_buffer_size;
#ifdef _DBG_
                cout << "epl::Vector::emplace_back(Args...) reallocated to new size: " << current_buffer_size << endl;
#endif
            }
            //
            // Copy constructing the object at the last location - check??!
            //
            _length++;
            new (_back)T{ std::forward<Args>(args)... };
#ifdef _DBG_ 
            cout << "epl::Vector::emplace_back(Args...) called. Pushed to: " << (_back - new_ptr) / sizeof(T) << endl;
#endif
            _back++;
            //
            // In case the buffer was reallocated, there are 2 things to do
            // Move construct the earlier object to the new buffer and
            // Destruct the earlier object
            //
            if (new_ptr != _buffer) {
                for (uint64_t k = 0; k < old_length; k++) {
                    new (_front + k) T{ std::move(old_front[k]) };
                    old_front[k].T::~T();
                }
                operator delete(_buffer);
                _buffer = new_ptr;
            }

            update_ctrlBlk(CtrlBlk::invalidate_reason::EMPLACE_BACK, (_back - 1), _front, _back);
        }
        
        void push_back(const T& val) {
            //
            // Adds a new value to the end of the array, using amortized doubling if the array
            // has to be resized. The argument is copy constructed.
            //
            T* new_ptr = _buffer, *old_front = _front, *old_back = _back;
            uint64_t current_buffer_size = (_buffer_end - _buffer), offset = 0, old_buffer_size, old_length = _length;
            old_buffer_size = current_buffer_size;

            if (_back == _buffer_end) {
                //
                // The next point to insert the object is past the last cell
                // Amortized doubling of the array is in order
                // In this case _front and _back do not change, as all new memory is to the end
                //
                current_buffer_size = current_buffer_size * 2;
                new_ptr = (T *) operator new ((size_t)current_buffer_size * sizeof(T));
                _front = new_ptr + (_front - _buffer);
                _back = new_ptr + (_back - _buffer);
                _buffer_end = new_ptr + current_buffer_size;
#ifdef _DBG_
                cout << "epl::Vector::push_back(const T& val) reallocated to new size: " << current_buffer_size << endl;
#endif
            }
            //
            // Copy constructing the object at the last location - check??!
            //
            _length++;
            new (_back) T{ val };
#ifdef _DBG_ 
            cout << "epl::Vector::push_back(const T& val) called. Pushed to: " << ((_back - new_ptr)/sizeof(T)) << endl;
#endif
            _back++;
            //
            // In case the buffer was reallocated, there are 2 things to do
            // Move construct the earlier object to the new buffer and
            // Destruct the earlier object
            //
            if (new_ptr != _buffer) {
                for (uint64_t k = 0; k < old_length; k++) {
                    new (_front + k) T{ std::move(old_front[k]) };
                    old_front[k].T::~T();
                }
                operator delete(_buffer);
                _buffer = new_ptr;
            }

            update_ctrlBlk(CtrlBlk::invalidate_reason::PUSH_BACK, (_back - 1), _front, _back);
        }
        
        void push_back(T&& val) {
            //
            // Same as above, but the argument is move constructed.
            //
            T* new_ptr = _buffer, *old_front = _front, *old_back = _back;
            uint64_t current_buffer_size = (_buffer_end - _buffer), offset = 0, old_buffer_size, old_length = _length;
            old_buffer_size = current_buffer_size;

            if (_back == _buffer_end) {
                //
                // The next point to insert the object is past the last cell
                // Amortized doubling of the array is in order
                // In this case _front and _back do not change, as all new memory is to the end
                //
                current_buffer_size = current_buffer_size * 2;
                new_ptr = (T *) operator new ((size_t)current_buffer_size * sizeof(T));
                _front = new_ptr + (_front - _buffer);
                _back = new_ptr + (_back - _buffer);
                _buffer_end = new_ptr + current_buffer_size;
#ifdef _DBG_
                cout << "epl::Vector::push_back(T&& val) reallocated to new size: " << current_buffer_size << endl;
#endif
            }
            //
            // Copy constructing the object at the last location - check??!
            //
            _length++;
            new (_back) T{ std::move(val) };
#ifdef _DBG_ 
            cout << "epl::Vector::push_back(T&& val) called. Pushed to: " << ((_back - new_ptr)/sizeof(T))  << endl;
#endif
            _back++;
            //
            // In case the buffer was reallocated, there are 2 things to do
            // Move construct the earlier objects to the new buffer and
            // Destruct the earlier objects
            //
            if (new_ptr != _buffer) {
                for (uint64_t k = 0; k < old_length; k++) {
                    new (_front + k) T{ std::move(old_front[k]) };
                    old_front[k].T::~T();
                }
                operator delete(_buffer);
                _buffer = new_ptr;
            }

            update_ctrlBlk(CtrlBlk::invalidate_reason::PUSH_BACK, (_back - 1), _front, _back);
        }
        
        void push_front(const T& val) {
            //
            // Similar to push_back but the element is added to the front of the Vector
            // The argument is copy constructed.
            //
            T* new_ptr = _buffer, *old_front = _front, *old_back = _back;
            uint64_t current_buffer_size = (_buffer_end - _buffer), offset = 0, old_buffer_size, old_length = _length;
            old_buffer_size = current_buffer_size;

            if (_front == _buffer) {
                //
                // The next point to insert the object is before the first cell
                // Amortized doubling of the array is in order
                // _front and _back increase by _current_buffer_size - old_buffer_size
                //
                current_buffer_size = current_buffer_size * 2;
                new_ptr = (T *) operator new ((size_t)current_buffer_size * sizeof(T));
                offset = current_buffer_size - old_buffer_size;
                _front = new_ptr + (_front - _buffer) + offset;
                _back = new_ptr + (_back - _buffer) + offset;
                _buffer_end = new_ptr + current_buffer_size;
#ifdef _DBG_
                cout << "epl::Vector::push_front(const T& val) reallocated to new size: " << current_buffer_size << endl;
#endif
            }
            //
            // Copy constructing the object at the first location
            //
            _front--;_length++;
            new (_front) T{ val };
#ifdef _DBG_ 
            cout << "epl::Vector::push_front(const T& val) called. Pushed to: " << ((_front - new_ptr)/sizeof(T)) << endl;
#endif
            //
            // In case the buffer was reallocated, there are 2 things to do
            // Move construct the earlier objects to the new buffer and
            // Destruct the earlier objects
            //
            if (new_ptr != _buffer) {
                for (uint64_t k = 0; k < old_length; k++) {
                    new (_front + k + 1) T{ std::move(old_front[k]) };
                    old_front[k].T::~T();
                }
                operator delete(_buffer);
                _buffer = new_ptr;
            }

            update_ctrlBlk(CtrlBlk::invalidate_reason::PUSH_FRONT, _front, _front, _back);
        }

        void push_front(T&& val) {
            //
            // Same as above, but the argument is move constructed.
            //
            T* new_ptr = _buffer, *old_front = _front, *old_back = _back;
            uint64_t current_buffer_size = (_buffer_end - _buffer), offset = 0, old_buffer_size, old_length = _length;
            old_buffer_size = current_buffer_size;

            if (_front == _buffer) {
                //
                // The next point to insert the object is before the first cell
                // Amortized doubling of the array is in order
                // _front and _back increase by _current_buffer_size - old_buffer_size
                //
                current_buffer_size = current_buffer_size * 2;
                new_ptr = (T *) operator new ((size_t)current_buffer_size * sizeof(T));
                offset = current_buffer_size - old_buffer_size;
                _front = new_ptr + (_front - _buffer) + offset;
                _back = new_ptr + (_back - _buffer) + offset;
                _buffer_end = new_ptr + current_buffer_size;
#ifdef _DBG_
                cout << "epl::Vector::push_front(T&& val) reallocated to new size: " << current_buffer_size << endl;
#endif
            }
            //
            // Copy constructing the object at the first location
            //
            _front--; _length++;
            new (_front) T{ std::move(val) };
#ifdef _DBG_ 
            cout << "epl::Vector::push_front(T&& val) called. Pushed to: " << ((_front - new_ptr)/sizeof(T))<< endl;
#endif
            //
            // In case the buffer was reallocated, there are 2 things to do
            // Move construct the earlier objects to the new buffer and
            // Destruct the earlier objects
            //
            if (new_ptr != _buffer) {
                for (uint64_t k = 0; k < old_length; k++) {
                    new (_front + k + 1) T{ std::move(old_front[k]) };
                    old_front[k].T::~T();
                }
                operator delete(_buffer);
                _buffer = new_ptr;
            }

            update_ctrlBlk(CtrlBlk::invalidate_reason::PUSH_FRONT, _front, _front, _back);
        }

        void pop_back(void) {
            //
            // Object at the end is destroyed, and length is updated (and any other pointers).
            // Storage is not reallocated, even if vector becomes empty. Capacity increases by 1. 
            // It is possible that a Vector has capacity at both the front and back simultaneously.
            // If the array is empty a std::out_of_range exception is thrown.
            //
            if (_length != 0) {
                _back--;
                _back[0].T::~T();
                _length--;
#ifdef _DBG_ 
                cout << "epl::Vector::pop_back() called. Popping from " << ((_back - _buffer)/sizeof(T)) + 1 << endl;
#endif
            }
            else {
                throw std::out_of_range("Cannot invoke pop_back() when the container is empty.");
            }

            update_ctrlBlk(CtrlBlk::invalidate_reason::POP_BACK, _back, _front, _back);
        }
        
        void pop_front(void) {
            //
            // Same as above, except that the element is removed from the front.
            //
            if (_length != 0) {
                _front[0].T::~T();
                 _front++;
                _length--;
#ifdef _DBG_ 
                cout << "epl::Vector::pop_back() called. Popping from " << ((_front - _buffer)/sizeof(T)) - 1 << endl;
#endif
            }
            else {
                throw std::out_of_range("Cannot invoke pop_front() when the container is empty.");
            }

            update_ctrlBlk(CtrlBlk::invalidate_reason::POP_FRONT, (_front - 1), _front, _back);
        }

    private:
        //
        // The array of objects currently in the Vector
        //
        T* _buffer;
        //
        // The current buffer end pointer, used during resizing operations
        //
        T* _buffer_end;
        //
        // The pointer to the first element at the front of the Vector
        //
        T* _front;
        //
        // The pointer to the last element at the back of the Vector
        //
        T* _back;
        //
        // The number of items in the array: should be equal = _back - _front
        //
        uint64_t _length;
        //
        // A constant for tweaking the intial size of the buffer
        //
        static const uint64_t initial_size = 8;
        //
        // The control block for maintaining version across vectors
        // and iterators
        //
        CtrlBlk* _ctrlBlk;

        void alloc(size_t n) {
            //
            // Method for allocating memory, used by init and init_list constructor
            //
            _buffer = (T *) operator new ((size_t)n * sizeof(T));
            _buffer_end = _buffer + n;
            _front = _buffer;
            _back = _buffer;
        }
        
        void init(uint64_t n) {
            //
            // Private method for intializing the Vector class, used by both constructors
            //
            size_t size = static_cast<size_t>(n);
            if (size == 0) {
                //
                // This function only allocates space for
                // initial_size elements, but does not call the constructor for those elements.
                // Sets _current_buffer_size to initial_size.
                //
                alloc(initial_size);
                _front = _buffer + 0;
                _back = _buffer + 0;
                _buffer_end = _buffer + initial_size;
                _length = 0;
            }
            else {
                //
                // This array is actually FULL, with n elements. The state variables of the class are 
                // also appropriately initialized. Placement new is called for each item.
                //
                alloc(size);
                for (uint64_t k = 0; k < size; k++) {
                    new (_buffer + k) T{};
                }
                _buffer_end = _buffer + size;
                _front = _buffer + 0;
                _back = _buffer + size;
                _length = size;
            }
            //
            // Create a new control block with version = 1
            //
            _ctrlBlk = new CtrlBlk(1);
        }
        
        void update_ctrlBlk(typename CtrlBlk::invalidate_reason reason, T* location, T* begin, T* end) {
            //
            // This function is called by every mutator method before mutating
            // the vector. This will invalidate the current control block
            // and create a new control block for every mutation of the vector.
            //
            uint64_t version = _ctrlBlk->_version;
            if (_ctrlBlk->_refs == 0) {
                _ctrlBlk->_version = version + 1;
                _ctrlBlk->_reason = CtrlBlk::invalidate_reason::NONE;
                _ctrlBlk->_location = nullptr;
                _ctrlBlk->_begin = nullptr;
                _ctrlBlk->_end = nullptr;
            }
            else {
                _ctrlBlk->invalidate(reason, location, begin, end);
                _ctrlBlk = new CtrlBlk(version + 1);
            }
        }

        void copy(const Vector& other) {
            //
            // Private method for copying the state of another object
            //
            this->_length = other._length;
            this->_buffer = (T *) operator new ((size_t)((other._buffer_end - other._buffer) * sizeof(T)));
            this->_buffer_end = (other._buffer_end - other._buffer) + this->_buffer;
            this->_front = (other._front - other._buffer) + this->_buffer;
            this->_back = (other._back - other._buffer) + this->_buffer;
            //
            // Do not copy the control block while copying, create a new control block for this guy
            //
            this->_ctrlBlk = new CtrlBlk(1);

            //
            // Copy construction of each of the elements from the first index to the last
            // This is called placement new, using copy constructor
            //
            for (uint64_t k = 0; k < this->_length; k++) {
                new (this->_front + k) T{ other._front[k] };
            }
        }

        void destroy(typename CtrlBlk::invalidate_reason reason) {
            //
            // Private method for destroying the state of an object
            //
            if (_buffer != nullptr) {
                if (_length > 0) {
                    for (uint64_t k = 0; k < _length; k++) {
                        //
                        // Run the destructor for all the elements that are currently in the Vector
                        //
                        _front[k].T::~T();
                    }
                }
                //
                // Calling delete using function syntax on _buffer, which is equivalent to calling free
                //
                operator delete(_buffer);
                _buffer = nullptr;
            }
            //
            // If the number of iterators is 0, then delete the control block
            //
            if (_ctrlBlk != nullptr) {
                //
                // Invalidate the control block in any case
                //
                _ctrlBlk->invalidate(reason, nullptr, nullptr, nullptr);
                if (_ctrlBlk->_refs == 0) {
                    delete(_ctrlBlk);
                    _ctrlBlk = nullptr;
                }
            }
        }
    };
}

#endif