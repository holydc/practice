#include <algorithm>
#include <functional>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

template<class E>
class Error : public std::exception {
public:
    Error(std::string what) : what_(E::tag() + std::string(": ") + std::move(what)) {
    }

    const char *what() const noexcept override {
        return what_.c_str();
    }

private:
    std::string what_;
};

struct IndexError {
    static const char *tag() { return "IndexError"; }
};

struct TypeError {
    static const char *tag() { return "TypeError"; }
};

struct ValueError {
    static const char *tag() { return "ValueError"; }
};

template<class T>
class ndarray {
public:
    typedef T dtype;

    static ndarray arange(int n, int start = 0) {
        std::vector<T> ary(n);
        std::iota(ary.begin(), ary.end(), start);

        auto data = transform_data(ary);

        return ndarray({n}, std::move(data));
    }

    static ndarray scalar(const T &s) {
        return ndarray({}, {std::make_shared<T>(s)});
    }

    template<template<class, class...> class Container, class... Ts>
    static ndarray full(const Container<int, Ts...> &shape, const T &fill_value) {
        int size = get_size(shape);
        auto data = transform_data(std::vector<T>(size, fill_value));
        return ndarray(Shape(shape.begin(), shape.end()), std::move(data));
    }

    static ndarray full(std::initializer_list<int> shape, const T &fill_value) {
        ndarray (*delegate)(const std::initializer_list<int> &, const T &) = &ndarray::full;
        return delegate(shape, fill_value);
    }

    ndarray() : shape_({0}) {
    }

    ndarray(std::initializer_list<ndarray> ary) {
        auto shape = get_shape(ary);
        if (shape.empty()) {
            return;
        }

        auto data = flatten_data(ary);
        if (data.empty()) {
            return;
        }

        shape_ = std::move(shape);
        data_ = std::move(data);
    }

    ndarray(std::initializer_list<T> ary) {
        Shape shape = {static_cast<int>(ary.size())};

        auto data = transform_data(ary);

        shape_ = std::move(shape);
        data_ = std::move(data);
    }

    ndarray(const ndarray &) = default;
    ndarray(ndarray &&) = default;
    ~ndarray() = default;

    ndarray &operator=(const ndarray &rhs) {
        const Shape &lshape = shape(), &rshape = rhs.shape();
        Data ldata = data_, rdata = rhs.data_;
        Shape shape;
        auto retval = braodcast(lshape, rshape, false, ldata, rdata, shape);
        if (!retval) {
            std::ostringstream oss;
            oss << "could not broadcast input array from shape ";
            dump_shape(oss, rshape);
            oss << " into shape ";
            dump_shape(oss, lshape);
            throw Error<ValueError>(oss.str());
        }

        // assert(data_.size() == rdata.size());

        for (size_t i = 0; i < data_.size(); ++i) {
            *data_[i] = *rdata[i];
        }

        return *this;
    }

    ndarray &operator=(ndarray &&rhs) {
        return (*this) = rhs;
    }

    template<class U>
    ndarray &opeartor=(const U &rhs) {
        return (*this) = ndarray::scalar(static_cast<T>(rhs));
    }

    const std::vector<int> &shape() const {
        return shape_;
    }

    int ndim() const {
        return shape_.size();
    }

    int size() const {
        return data_.size();
    }

    int len() const {
        if (ndim() == 0) {
            throw Error<TypeError>("scalar type has no len()");
        }

        return shape_.front();
    }

    template<template<class, class...> class Container, class... Ts>
    ndarray reshape(const Container<int, Ts...> &new_shape) const {
        // TODO negative value for unknown dimension.
        int size = get_size(shape_);
        if (size != get_size(new_shape)) {
            std::ostringstream oss;
            oss << "cannot reshape array of size " << size << " into shape (";
            std::copy(new_shape.begin(), new_shape.end(), std::ostream_iterator<int>(oss, ","));
            oss << ")";
            throw Error<ValueError>(oss.str());
        }

        return ndarray(Shape(new_shape.begin(), new_shape.end()), data_);
    }

    ndarray reshape(std::initializer_list<int> new_shape) const {
        ndarray (ndarray::*delegate)(const std::initializer_list<int> &) const = &ndarray::reshape;
        return (this->*delegate)(new_shape);
    }

    template<class U>
    ndarray<U> astype() const {
        std::vector<std::shared_ptr<U>> data(size());
        std::transform(data_.begin(), data_.end(), data.begin(), [] (Value arg) -> std::shared_ptr<U> {
            return std::make_shared<U>(static_cast<U>(*arg));
        });
        return ndarray<U>(shape_, data);
    }

    ndarray operator[](int index) const {
        index = resolve_index(index, true);

        Shape shape(std::next(shape_.begin()), shape_.end());
        int size = get_size(shape);
        index *= size;
        return ndarray(std::move(shape), Data(
                std::next(data_.begin(), index),
                std::next(data_.begin(), index + size)));
    }

    /**
     * Convert to scalar.
     */
    operator T() const {
        if (size() != 1) {
            throw Error<TypeError>("only size-1 arrays can be converted to scalars");
        }

        return *data_.front();
    }

    /**
     * Perform Python array slicing.
     *
     * Python notation "a[1:2, 3, 4:5]" becomes "a.slice(std::make_pair(1, 2), 3, std::make_pair(4, 5))".
     */
    template<class... Args>
    ndarray slice(Args... args) const {
        if (sizeof...(args) > ndim()) {
            throw Error<IndexError>("too many indices for array");
        }

        return slice_impl(std::move(args)...);
    }

    ndarray operator-() const {
        return operator_impl(ndarray::scalar(-1), OP_MUL);
    }

private:
    typedef std::vector<int> Shape;
    typedef std::shared_ptr<dtype> Value;
    typedef std::vector<Value> Data;

    enum Operator {
        OP_ADD, OP_SUB, OP_MUL, OP_DIV
    };

    template<class U>
    friend class ndarray;

    friend std::ostream &operator<<(std::ostream &os, const ndarray &ary) {
        if (ary.ndim() == 0) { // scalar, not array
            if (ary.size() == 1) {
                os << static_cast<T>(ary);
            } else {
                std::cerr << "ndarray: scalar, data size should be 1 but get " << ary.size() << std::endl;
            }
            return os;
        }

        os << "array(";

        std::vector<int> s;
        size_t i = 0;
        do {
            for (; s.size() < ary.ndim(); s.push_back(ary.shape_[s.size()])) {
                os << "[";
            }
            for (; s.back() > 0; --s.back()) {
                os << *(ary.data_[i]) << ',';
                ++i;
            }
            for (; s.back() == 0; --s.back()) {
                os << "],";
                s.pop_back();
                if (s.empty()) {
                    // assert(i == ary.data_.size());
                    break;
                }
            }
        } while (!s.empty());

        os << ")";
        return os;
    }

    template<class U>
    friend ndarray operator+(const U &lhs, const ndarray &rhs) {
        return ndarray::scalar(static_cast<T>(lhs)).operator_impl(rhs, ndarray::OP_ADD);
    }

    template<class U>
    friend ndarray operator+(const ndarray &lhs, const U &rhs) {
        return lhs.operator_impl(ndarray::scalar(static_cast<T>(rhs)), ndarray::OP_ADD);
    }

    friend ndarray operator+(const ndarray &lhs, const ndarray &rhs) {
        return lhs.operator_impl(rhs, ndarray::OP_ADD);
    }

    template<class U>
    friend ndarray operator-(const U &lhs, const ndarray &rhs) {
        return ndarray::scalar(static_cast<T>(lhs)).operator_impl(rhs, ndarray::OP_SUB);
    }

    template<class U>
    friend ndarray operator-(const ndarray &lhs, const U &rhs) {
        return lhs.operator_impl(ndarray::scalar(static_cast<T>(rhs)), ndarray::OP_SUB);
    }

    friend ndarray operator-(const ndarray &lhs, const ndarray &rhs) {
        return lhs.operator_impl(rhs, ndarray::OP_SUB);
    }

    template<class U>
    friend ndarray operator*(const U &lhs, const ndarray &rhs) {
        return ndarray::scalar(static_cast<T>(lhs)).operator_impl(rhs, ndarray::OP_MUL);
    }

    template<class U>
    friend ndarray operator*(const ndarray &lhs, const U &rhs) {
        return lhs.operator_impl(ndarray::scalar(static_cast<T>(rhs)), ndarray::OP_MUL);
    }

    friend ndarray operator*(const ndarray &lhs, const ndarray &rhs) {
        return lhs.operator_impl(rhs, ndarray::OP_MUL);
    }

    template<class U>
    friend ndarray operator/(const U &lhs, const ndarray &rhs) {
        return ndarray::scalar(static_cast<T>(lhs)).operator_impl(rhs, ndarray::OP_DIV);
    }

    template<class U>
    friend ndarray operator/(const ndarray &lhs, const U &rhs) {
        return lhs.operator_impl(ndarray::scalar(static_cast<T>(rhs)), ndarray::OP_DIV);
    }

    friend ndarray operator/(const ndarray &lhs, const ndarray &rhs) {
        return lhs.operator_impl(rhs, ndarray::OP_DIV);
    }

    template<template<class, class...> class Container, class... Ts>
    static Shape get_shape(const Container<ndarray, Ts...> &ary) {
        if (ary.size() == 0) {
            return {};
        }

        auto shape = ary.begin()->shape();

        auto not_same_shape = [&shape] (const ndarray &subary) -> bool {
            return subary.shape() != shape;
        };

        if (std::any_of(ary.begin(), ary.end(), not_same_shape)) {
            // TODO dtype=object
            std::cerr << "ndarray::get_shape: shape not match" << std::endl;
            return {};
        }

        shape.insert(shape.begin(), ary.size());
        return shape;
    }

    template<template<class, class...> class Container, class... Ts>
    static int get_size(const Container<int, Ts...> &shape) {
        return std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<int>());
    }

    template<template<class, class...> class Container, class... Ts>
    static Data flatten_data(const Container<ndarray, Ts...> &ary) {
        auto flatten = [] (auto &&data, const ndarray &subary) -> Data {
            data.insert(data.end(), subary.data_.begin(), subary.data_.end());
            return data;
        };

        return std::accumulate(ary.begin(), ary.end(), Data(), flatten);
    }

    template<template<class, class...> class Container, class... Ts>
    static Data transform_data(const Container<T, Ts...> &ary) {
        Data data(ary.size());
        std::transform(ary.begin(), ary.end(), data.begin(), &std::make_shared<T, const T &>);
        return data;
    }

    static std::ostream &dump_shape(std::ostream &os, const Shape &shape) {
        os << '(';
        std::copy(shape.begin(), shape.end(), std::ostream_iterator<int>(os, ","));
        os << ')';
        return os;
    }

    /**
     * Repeat data groups of n elements k times.
     */
    static Data expand_data(const Data &old_data, int n, int k) {
        Data new_data;
        for (auto begin = old_data.begin(); begin != old_data.end(); std::advance(begin, n)) {
            auto end = std::next(begin, n);
            for (int i = 0; i < k; ++i) {
                new_data.insert(new_data.end(), begin, end);
            }
        }
        return new_data;
    }

    static bool braodcast(
            const Shape &lshape,
            const Shape &rshape,
            bool is_lhs_mutable,
            Data &ldata,
            Data &rdata,
            Shape &shape) {
        int lsize = 1, rsize = 1;
        for (auto i = lshape.rbegin(), j = rshape.rbegin(); (i != lshape.rend()) || (j != rshape.rend());) {
            int ldim = 1, rdim = 1;
            if (i != lshape.rend()) {
                ldim = *i;
                ++i;
            }
            if (j != rshape.rend()) {
                rdim = *j;
                ++j;
            }
            if (ldim != rdim) {
                if (rdim == 1) {
                    rdim = ldim;
                    rdata = expand_data(rdata, rsize, rdim);
                } else if (is_lhs_mutable && (ldim == 1)) {
                    ldim = rdim;
                    ldata = expand_data(ldata, lsize, ldim);
                } else {
                    return false;
                }
            }
            lsize *= ldim;
            rsize *= rdim;
            shape.push_back(ldim);
        }
        std::reverse(shape.begin(), shape.end());
        return true;
    }

    ndarray(Shape shape, Data data)
            : shape_(std::move(shape)),
              data_(std::move(data)) {
    }

    int resolve_index(int index, bool verify) const {
        if (ndim() == 0) {
            throw Error<IndexError>("invalid index to scalar variable");
        }

        int dim = len();

        if (verify && ((index < -dim) || (index >= dim))) {
            std::ostringstream oss;
            oss << "index " << index << " is out of bounds for axis 0 with size " << dim;
            throw Error<IndexError>(oss.str());
        }

        return (index < 0) ? (index + dim) : index;
    }

    template<class... Args>
    ndarray slice_impl(int index, Args... args) const {
        return (*this)[index].slice(std::move(args)...);
    }

    template<class... Args>
    ndarray slice_impl(std::pair<int, int> range, Args... args) const {
        std::vector<ndarray> slices;
        int begin = std::max(0, resolve_index(range.first, false));
        int end = std::min(len(), resolve_index(range.second, false));
        for (; begin < end; ++begin) {
            slices.push_back(slice_impl(begin, std::move(args)...));
        }

        auto shape = get_shape(slices);
        if (shape.empty()) {
            return {};
        }

        auto data = flatten_data(slices);
        if (data.empty()) {
            return {};
        }

        return ndarray(std::move(shape), std::move(data));
    }

    ndarray slice_impl() const {
        return *this;
    }

    ndarray operator_impl(const ndarray &rhs, Operator op) const {
        const Shape &lshape = shape(), &rshape = rhs.shape();
        Data ldata = data_, rdata = rhs.data_;
        Shape shape;
        auto retval = braodcast(lshape, rshape, true, ldata, rdata, shape);
        if (!retval) {
            std::ostringstream oss;
            oss << "operands could not be broadcast together with shapes ";
            dump_shape(oss, lshape);
            oss << " ";
            dump_shape(oss, rshape);
            throw Error<ValueError>(oss.str());
        }

        // assert(ldata.size() == rdata.size())

        std::function<T(const T &, const T &)> f;
        switch (op) {
        case OP_ADD:
            f = [] (const T &lhs, const T &rhs) -> T { return lhs + rhs; };
            break;
        case OP_SUB:
            f = [] (const T &lhs, const T &rhs) -> T { return lhs - rhs; };
            break;
        case OP_MUL:
            f = [] (const T &lhs, const T &rhs) -> T { return lhs * rhs; };
            break;
        case OP_DIV:
            f = [] (const T &lhs, const T &rhs) -> T { return lhs / rhs; };
            break;
        default:
            throw Error<TypeError>("unsupported operator");
        }

        Data data(ldata.size());
        for (size_t i = 0; i < ldata.size(); ++i) {
            data[i] = std::make_shared<T>(f(*ldata[i], *rdata[i]));
        }

        return ndarray(std::move(shape), std::move(data));
    }

    Shape shape_;
    Data data_;
};

int main() {
    std::cout << ">>> a = np.arange(20).reshape(4, 1, 5)" << std::endl;
    auto a = ndarray<int>::arange(20).reshape({4, 1, 5});
    std::cout << ">>> a" << std::endl;
    std::cout << a << std::endl;
    // array([[[ 0,  1,  2,  3,  4]],
    //        [[ 5,  6,  7,  8,  9]],
    //        [[10, 11, 12, 13, 14]],
    //        [[15, 16, 17, 18, 19]]])
    std::cout << ">>> b = a[1:4, 0, 2:5]" << std::endl;
    auto b = a.slice(std::make_pair(1, 4), 0, std::make_pair(2, 5));
    std::cout << ">>> b" << std::endl;
    std::cout << b << std::endl;
    // array([[ 7,  8,  9],
    //        [12, 13, 14],
    //        [17, 18, 19]])
    std::cout << ">>> a[1:4, 0:1, 2:5] = 3 + np.full((3, 1, 1), 1) + -np.full((1, 3), 2)" << std::endl;
    a.slice(std::make_pair(1, 4), std::make_pair(0, 1), std::make_pair(2, 5)) =
            3 + ndarray<int>::full({3, 1, 1}, 1) + -ndarray<int>::full({1, 3}, 2);
    std::cout << ">>> a" << std::endl;
    std::cout << a << std::endl;
    // array([[[ 0,  1,  2,  3,  4]],
    //        [[ 5,  6,  2,  2,  2]],
    //        [[10, 11,  2,  2,  2]],
    //        [[15, 16,  2,  2,  2]]])
    std::cout << ">>> b" << std::endl;
    std::cout << b << std::endl;
    // array([[2, 2, 2],
    //        [2, 2, 2],
    //        [2, 2, 2]])
    std::cout << ">>> a[-1][-1][-1] = 5566" << std::endl;
    a[-1][-1][-1] = 5566;
    std::cout << ">>> a" << std::endl;
    std::cout << a << std::endl;
    // array([[[   0,    1,    2,    3,    4]],
    //        [[   5,    6,    2,    2,    2]],
    //        [[  10,   11,    2,    2,    2]],
    //        [[  15,   16,    2,    2, 5566]]])
    std::cout << ">>> b" << std::endl;
    std::cout << b << std::endl;
    // array([[   2,    2,    2],
    //        [   2,    2,    2],
    //        [   2,    2, 5566]])

    return 0;
}
