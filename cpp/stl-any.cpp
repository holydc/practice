#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>

// BadAnyCast

struct BadAnyCast {
  std::string what;

  BadAnyCast(std::string what) : what(std::move(what)) {
  }
}; // struct BadAnyCast

// Any

class Any {
public:
  constexpr Any() noexcept = default;

  Any(const Any &other) : data_(other.data_->clone()) {
  }

  Any(Any &&other) noexcept {
    swap(other);
  }

  template<class ValueType>
  Any(ValueType &&value) {
    emplace<ValueType>(std::forward<ValueType>(value));
  }

  //template<class ValueType, class... Args>
  //explicit Any(std::in_place_type_t<ValueType>, Args&&...);

  //template<class ValueType, class U, class... Args>
  //explicit Any(std::in_place_type_t<ValueType>, std::initializer_list<U>, Args&&...);

  Any &operator=(const Any &rhs) {
    Any(rhs).swap(*this);
    return *this;
  }

  Any &operator=(Any &&rhs) noexcept {
    Any(std::move(rhs)).swap(*this);
    return *this;
  }

  template<class ValueType>
  Any &operator=(ValueType &&rhs) {
    Any(std::forward<ValueType>(rhs)).swap(*this);
    return *this;
  }

  ~Any() = default;

  template<class ValueType, class... Args>
  std::decay_t<ValueType> &emplace(Args&&... args) {
    // Throws any exception thrown by ValueType's constructor.
    // If an exception is thrown, the previously contained object (if any) has been destroyed,
    // and *this does not contain a value.
    reset();

    auto data_p = new any_t<std::decay_t<ValueType>>(std::forward<Args>(args)...);
    auto &data = data_p->data;
    data_.reset(data_p);
    return data;
  }

  template<class ValueType, class U, class... Args>
  std::decay_t<ValueType> &emplace(std::initializer_list<U> il, Args&&... args) {
    // Throws any exception thrown by ValueType's constructor.
    // If an exception is thrown, the previously contained object (if any) has been destroyed,
    // and *this does not contain a value.
    reset();

    auto data_p = new any_t<std::decay_t<ValueType>>(il, std::forward<Args>(args)...);
    auto &data = data_p->data;
    data_.reset(data_p);
    return data;
  }

  void reset() noexcept {
    data_.reset();
  }

  void swap(Any &other) noexcept {
    std::swap(data_, other.data_);
  }

  bool hasValue() const noexcept {
    return data_ != nullptr;
  }

  const std::type_info &type() const noexcept {
    return data_->getType();
  }

private:
  template<class ValueType>
  friend ValueType anyCast(Any &);

  template<class ValueType>
  friend ValueType anyCast(Any &&);

  template<class ValueType>
  friend ValueType anyCast(const Any &);

  template<class ValueType>
  friend ValueType *anyCast(Any *);

  template<class ValueType>
  friend const ValueType *anyCast(const Any *);

  struct any_i {
    virtual ~any_i() = default;

    virtual any_i *clone() const = 0;
    virtual const std::type_info &getType() const noexcept = 0;
  }; // struct any_i

  template<class ValueType>
  struct any_t : public any_i {
    ValueType data;
    const std::type_info *type;

    any_t(const ValueType &data) : data(data) {
      setType();
    }

    any_t(ValueType &&data) : data(std::move(data)) {
      setType();
    }

    template<class... Args>
    any_t(Args&&... args) : data(std::forward<Args>(args)...) {
      setType();
    }

    template<class U, class... Args>
    any_t(std::initializer_list<U> il, Args&&... args) : data(il, std::forward<Args>(args)...) {
    }

    any_i *clone() const override {
      return new any_t(data);
    }

    const std::type_info &getType() const noexcept override {
      return *type;
    }

    void setType() {
      type = &typeid(ValueType);
    }
  }; // struct any_t

  std::unique_ptr<any_i> data_;
}; // class Any

// anyCast

template<class ValueType>
ValueType anyCast(Any &operand) {
  auto data_p = anyCast<std::remove_cv_t<std::remove_reference_t<ValueType>>>(&operand);
  if (data_p == nullptr) {
    throw BadAnyCast("Bad anyCast");
  }
  return static_cast<ValueType>(*data_p);
}

template<class ValueType>
ValueType anyCast(Any &&operand) {
  return static_cast<ValueType>(anyCast<std::remove_cv_t<std::remove_reference_t<ValueType>> &&>(operand));
}

template<class ValueType>
ValueType anyCast(const Any &operand) {
  return anyCast<std::remove_reference_t<ValueType> &>(const_cast<Any &>(operand));
}

template<class ValueType>
ValueType *anyCast(Any *operand) {
  if (operand == nullptr) {
    return nullptr;
  }
  auto data_p = dynamic_cast<Any::any_t<std::remove_cv_t<ValueType>> *>(operand->data_.get());
  if (data_p == nullptr) {
    return nullptr;
  }
  return &(data_p->data);
}

template<class ValueType>
const ValueType *anyCast(const Any *operand) {
  return anyCast<ValueType>(const_cast<Any *>(operand));
}

// example

int main() {
  // simple example

  auto a = Any(12);
  std::cout << a.type().name() << std::endl;

  std::cout << anyCast<int>(a) << std::endl;

  try {
    std::cout << anyCast<std::string>(a) << std::endl;
  } catch (BadAnyCast e) {
    std::cout << e.what << std::endl;
  }

  // advanced example

  a = std::string("hello");
  std::cout << a.type().name() << std::endl;
  std::cout << anyCast<std::string>(a) << std::endl;

  auto &ra = anyCast<std::string &>(a); //< reference
  ra[1] = 'o';

  std::cout << "a: " << anyCast<const std::string &>(a) << std::endl; //< const reference

  auto b = anyCast<std::string &&>(a); //< rvalue reference (no need for std::move)

  // Note, 'b' is a move-constructed std::string, 'a' is now empty

  std::cout << "a: " << *anyCast<std::string>(&a) << std::endl;
  std::cout << "b: " << b << std::endl;

  return 0;
}