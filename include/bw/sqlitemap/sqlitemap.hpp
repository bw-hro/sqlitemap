// sqlitemap
// SPDX-FileCopyrightText: 2024-present Benno Waldhauer
// SPDX-License-Identifier: MIT

#pragma once

#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>

#include <sqlite3.h>

namespace bw::sqlitemap
{

using blob = std::vector<std::byte>;

namespace codecs
{

struct input_type_placeholder
{
};
inline std::string to_string(const input_type_placeholder& input);

} // namespace codecs

class sqlitemap_error : public std::runtime_error
{
  public:
    sqlitemap_error(const std::string& msg)
        : std::runtime_error("sqlitemap_error - " + msg) {};
};

enum log_level
{
    off = 6000,
    error = 5000,
    warn = 4000,
    info = 3000,
    debug = 2000,
    trace = 1000
};

class logger
{
  public:
    using log_function = std::function<void(log_level, const std::string&)>;

    void error(const std::string& msg) const
    {
        log(log_level::error, msg);
    }

    void warn(const std::string& msg) const
    {
        log(log_level::warn, msg);
    }

    void info(const std::string& msg) const
    {
        log(log_level::info, msg);
    }

    void debug(const std::string& msg) const
    {
        log(log_level::debug, msg);
    }

    void trace(const std::string& msg) const
    {
        log(log_level::trace, msg);
    }

    void log(log_level level, const std::string& msg) const
    {
        if (level < custom_log_level)
            return;

        if (custom_log_impl)
            return custom_log_impl(level, msg);

        default_log_impl(level, msg);
    }

    void register_log_impl(log_function log_impl)
    {
        custom_log_impl = log_impl;
    }

    void set_level(log_level level)
    {
        custom_log_level = level;
    }

    log_level get_level() const
    {
        return custom_log_level;
    }

  private:
    void default_log_impl(log_level level, const std::string& msg) const
    {
        std::lock_guard<std::mutex> lg(log_mutex);
        switch (level)
        {
        case log_level::error:
        case log_level::warn:
            std::cerr << msg << std::endl;
            break;
        default:
            std::cout << msg << std::endl;
        }
    }

    static inline std::mutex log_mutex;
    log_function custom_log_impl;
    log_level custom_log_level = log_level::debug;
};

namespace details
{

template <typename T> struct can_convert_to_string_std
{
  private:
    // If to_string() is available for type U, return true
    template <typename U>
    static auto test(U*) -> decltype(std::to_string(std::declval<U>()), std::true_type());

    // Fallback when no match is found
    template <typename> static std::false_type test(...);

  public:
    static constexpr bool value = decltype(test<T>(nullptr))::value;
};

template <typename T> struct can_convert_to_string_sm
{
  private:
    // If to_string() is available for type U, return true
    template <typename U>
    static auto test(U*)
        -> decltype(bw::sqlitemap::codecs::to_string(std::declval<U>()), std::true_type());

    // Fallback when no match is found
    template <typename> static std::false_type test(...);

  public:
    static constexpr bool value = decltype(test<T>(nullptr))::value;
};

template <typename T>
std::string as_string_or(const T& value,
                         const std::string& fallback = std::string("type:") + typeid(T).name())
{
    if constexpr (can_convert_to_string_sm<T>::value)
    {
        return bw::sqlitemap::codecs::to_string(value);
    }
    else if constexpr (can_convert_to_string_std<T>::value)
    {
        return std::to_string(value);
    }
    else if constexpr (std::is_same<T, std::string>::value)
    {
        return value;
    }
    else if constexpr (std::is_same<T, const char*>::value)
    {
        return value;
    }
    else
    {
        return fallback;
    }
}

inline void require_return_code(int rc, int rc_expected, const std::string& message,
                                sqlite3* db = nullptr)
{
    if (rc != rc_expected)
    {
        auto msg = message;
        if (msg.empty())
        {
            auto e = std::to_string(rc_expected);
            auto a = std::to_string(rc);
            msg = "Statement failed. Expect return code " + e + " but was " + a;
        }

        auto sqlite_err = db ? sqlite3_errmsg(db) : "";
        throw sqlitemap_error(msg + (db ? " - sqlite3_errmsg: " : "") + sqlite_err);
    }
}

inline void require_return_code(int rc, int rc_expected, sqlite3* db = nullptr)
{
    require_return_code(rc, rc_expected, "", db);
}

inline void check_ok(int rc, const std::string& message, sqlite3* db = nullptr)
{
    require_return_code(rc, SQLITE_OK, message, db);
}

inline void check_ok(int rc, sqlite3* db = nullptr)
{
    check_ok(rc, "", db);
}

inline void check_done(int rc, const std::string& message, sqlite3* db = nullptr)
{
    require_return_code(rc, SQLITE_DONE, message, db);
}

inline void check_done(int rc, sqlite3* db = nullptr)
{
    check_done(rc, "", db);
}

template <typename T> constexpr bool has_native_sqlite_support()
{
    return std::is_same_v<T, std::string> || std::is_integral_v<T> || std::is_floating_point_v<T> ||
           std::is_same_v<T, std::nullptr_t> || std::is_same_v<T, blob>;
}

template <typename T> T column_value(sqlite3_stmt* stmt, int index)
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, index));
        if (text)
            return std::string(text);
        else
            return "";
    }
    else if constexpr (std::is_integral_v<T>)
    {
        if constexpr (sizeof(T) <= sizeof(int))
        {
            return static_cast<T>(sqlite3_column_int(stmt, index));
        }
        else
        {
            return static_cast<T>(sqlite3_column_int64(stmt, index));
        }
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        return static_cast<T>(sqlite3_column_double(stmt, index));
    }
    else if constexpr (std::is_same_v<T, blob>)
    {
        const void* data = sqlite3_column_blob(stmt, index);

        if (data == nullptr)
            return blob();

        int num_bytes = sqlite3_column_bytes(stmt, index);
        blob result(num_bytes);
        std::memcpy(result.data(), data, num_bytes);

        return result;
    }
    else if constexpr (std::is_same_v<T, std::nullptr_t>)
    {
        return nullptr; // for null values, just return nullptr
    }
    else
    {
        static_assert(has_native_sqlite_support<T>(), "Unsupported type for sqlite_column_value.");
    }
}

template <typename T> int bind_param(sqlite3_stmt* stmt, int index, const T& value)
{
    if constexpr (std::is_same_v<T, std::string>)
    {
        return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }
    else if constexpr (std::is_integral_v<T>)
    {
        if constexpr (sizeof(T) <= sizeof(int))
        {
            return sqlite3_bind_int(stmt, index, static_cast<int>(value));
        }
        else
        {
            return sqlite3_bind_int64(stmt, index, static_cast<sqlite3_int64>(value));
        }
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        return sqlite3_bind_double(stmt, index, static_cast<double>(value));
    }
    else if constexpr (std::is_same_v<T, blob>)
    {
        return sqlite3_bind_blob(stmt, index, value.data(), value.size(), SQLITE_TRANSIENT);
    }
    else if constexpr (std::is_same_v<T, std::nullptr_t>)
    {
        return sqlite3_bind_null(stmt, index);
    }
    else
    {
        // compile time error for not supported types
        static_assert(has_native_sqlite_support<T>(),
                      "Unsupported type for sqlite3_bind. Supported types are std::string, "
                      "integral types, floating-point types, and std::nullptr_t.");
        return SQLITE_ERROR; // should never be reached
    }
}

template <typename T>
int bind_param_checked(sqlite3_stmt* stmt, int index, const T& value,
                       const std::string& message = "", sqlite3* db = nullptr)
{
    int rc = bind_param(stmt, index, value);
    check_ok(rc, message, db);
    return rc;
}

inline int prepare_checked(sqlite3* db, const std::string& sql, sqlite3_stmt** stmt)
{
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, stmt, nullptr);
    check_ok(rc, "Failed to prepare statement", db);
    return rc;
}

inline int exec_checked(sqlite3* db, const std::string& sql,
                        int (*callback)(void*, int, char**, char**) = nullptr,
                        void* first_arg_to_callback = nullptr)
{
    int rc = sqlite3_exec(db, sql.c_str(), callback, first_arg_to_callback, nullptr);
    check_ok(rc, "Failed to execute statement", db);
    return rc;
}

// Base template for function traits
template <typename Func> struct function_traits;

// Specialization for function pointers
template <typename R, typename Arg> struct function_traits<R (*)(Arg)>
{
    using return_type = R;
    using argument_type = Arg;
};

// Specialization for lambdas and functors
template <typename F> struct function_traits : function_traits<decltype(&F::operator())>
{
};

// Specialization for lambdas with const operator()
template <typename R, typename C, typename Arg> struct function_traits<R (C::*)(Arg) const>
{
    using return_type = R;
    using argument_type = Arg;
};

// Ensure specialization for non-const lambdas
template <typename R, typename C, typename Arg> struct function_traits<R (C::*)(Arg)>
{
    using return_type = R;
    using argument_type = Arg;
};

// Generates a unique name for the temporary sqlite database.
static std::string generate_temp_filename()
{
    using namespace std::chrono;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(10000, 99999);
    int random_number = dist(gen);

    auto now = system_clock::now();
    auto timestamp = duration_cast<milliseconds>(now.time_since_epoch()).count();

    std::string prefix = "sqlitemap";
    std::string ts = std::to_string(timestamp);
    std::string rn = std::to_string(random_number);
    return prefix + "_" + ts + "_" + rn;
}

} // namespace details

namespace codecs
{

enum class sqlite_storage_class
{
    INTEGER,
    REAL,
    TEXT,
    BLOB
};

template <typename T> constexpr sqlite_storage_class sqlite_storage_class_from_type()
{
    if constexpr (std::is_integral_v<T>)
    {
        return sqlite_storage_class::INTEGER;
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        return sqlite_storage_class::REAL;
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        return sqlite_storage_class::TEXT;
    }
    else if constexpr (std::is_same_v<T, const char*>)
    {
        return sqlite_storage_class::TEXT;
    }
    else if constexpr (std::is_same_v<T, blob>)
    {
        return sqlite_storage_class::BLOB;
    }
}

inline std::string to_string(sqlite_storage_class sc)
{
    switch (sc)
    {
    case sqlite_storage_class::INTEGER:
        return "INTEGER";
    case sqlite_storage_class::REAL:
        return "REAL";
    case sqlite_storage_class::TEXT:
        return "TEXT";
    case sqlite_storage_class::BLOB:
        return "BLOB";
    default:
        return "UNKOWN";
    }
}

// identity function, just returns the input
template <typename T> T identity(const T& input)
{
    return input;
}

/**
 * @class codec
 * @brief Base class for encoding and decoding operations.
 *
 * This template struct provides a common interface for encoding and decoding
 * operations, which are used for converting between different data types.
 *
 * @tparam IN_T The input type for encoding.
 * @tparam OUT_T The output type for decoding.
 *
 * codec is a fundamental building block of the database management system,
 * providing the necessary functionality for encoding and decoding operations.
 */

template <typename IN_T, typename OUT_T> struct codec
{
    using in_type = IN_T;
    using out_type = OUT_T;

    const std::function<OUT_T(const IN_T&)> encode;
    const std::function<IN_T(const OUT_T&)> decode;
};

struct key_codec_tag
{
};

template <typename IN_T, typename OUT_T>
struct key_codec : public codec<IN_T, OUT_T>, public key_codec_tag
{
    using codec<IN_T, OUT_T>::codec;

    key_codec(std::function<OUT_T(const IN_T&)> encode, std::function<IN_T(const OUT_T&)> decode)
        : codec<IN_T, OUT_T>{encode, decode}
    {
    }
};

template <typename T> struct is_key_codec : std::false_type
{
    // Default case, not a key_codec specialization
};

template <typename IN_T, typename OUT_T>
struct is_key_codec<key_codec<IN_T, OUT_T>> : std::true_type
{
    // Specialization for key_codec<IN_T, OUT_T>
};

struct value_codec_tag
{
};

template <typename IN_T, typename OUT_T>
struct value_codec : public codec<IN_T, OUT_T>, public value_codec_tag
{
    using codec<IN_T, OUT_T>::codec;

    value_codec(std::function<OUT_T(const IN_T&)> encode, std::function<IN_T(const OUT_T&)> decode)
        : codec<IN_T, OUT_T>{encode, decode}
    {
    }
};

template <typename T> struct is_value_codec : std::false_type
{
    // Default case, not a value_codec specialization
};

template <typename IN_T, typename OUT_T>
struct is_value_codec<value_codec<IN_T, OUT_T>> : std::true_type
{
    // Specialization for value_codec<IN_T, OUT_T>
};

template <typename T> struct unknown_codec_tag : std::false_type
{
    // Define unknown_codec_tag for triggering static assertion
};

template <typename T, typename E, typename D> auto taged_codec_from(E encoder, D decoder)
{
    using ea_type = typename details::function_traits<E>::argument_type;
    using er_type = typename details::function_traits<E>::return_type;

    using da_type = typename details::function_traits<D>::argument_type;
    using dr_type = typename details::function_traits<D>::return_type;

    // Ensure that decode function's input type matches encode function's output type
    static_assert(std::is_same_v<std::decay_t<da_type>, std::decay_t<er_type>>,
                  "Decoder input type must match Encoder output type");

    // Ensure that decode function's output type matches the original type being encoded
    static_assert(std::is_same_v<std::decay_t<dr_type>, std::decay_t<ea_type>>,
                  "Decoder return type must match Encoder input type");

    if constexpr (std::is_same_v<T, key_codec_tag>)
    {
        return key_codec<std::decay_t<ea_type>, std::decay_t<er_type>>{encoder, decoder};
    }
    else if constexpr (std::is_same_v<T, value_codec_tag>)
    {
        return value_codec<std::decay_t<ea_type>, std::decay_t<er_type>>{encoder, decoder};
    }
    else
    {
        static_assert(unknown_codec_tag<T>::value, "Unknown codec tag type");
    }
}

template <typename TAG, typename TYPE> auto taged_codec_from()
{
    using namespace codecs;
    using ED = decltype(&identity<TYPE>);
    return taged_codec_from<TAG, ED, ED>(identity<TYPE>, identity<TYPE>);
}

/**
 * @class codec_pair
 * @brief Combines key and value codecs for database operations.
 *
 * This template struct pairs a key codec and a value codec, which are used
 * for encoding and decoding database keys and values, respectively.
 *
 * @tparam KC The key codec type.
 * @tparam VC The value codec type.
 *
 * codec_pair is a fundamental building block of the database management system,
 * providing the necessary codecs for key and value operations.
 */

template <typename KC, typename VC> struct codec_pair
{
    codec_pair(KC k, VC v)
        : key_codec(k)
        , value_codec(v)
    {
        static_assert(is_key_codec<std::decay_t<KC>>::value,
                      "KC must be a specialization of key_codec<IN_T, OUT_T>");

        static_assert(is_value_codec<std::decay_t<VC>>::value,
                      "VC must be a specialization of value_codec<IN_T, OUT_T>");
    }

    using key_in_type = typename KC::in_type;
    using key_out_type = typename KC::out_type;
    using value_in_type = typename VC::in_type;
    using value_out_type = typename VC::out_type;

    const KC key_codec;
    const VC value_codec;
};

template <typename T> struct is_codec_pair : std::false_type
{
    // Default case, not a codec_pair specialization
};

template <typename KC, typename VC> struct is_codec_pair<codec_pair<KC, VC>> : std::true_type
{
};

} // namespace codecs

template <typename E, typename D> auto key_codec(E encoder, D decoder)
{
    return codecs::taged_codec_from<codecs::key_codec_tag, E, D>(encoder, decoder);
}

// use identity function of type T to define a key codec
template <typename T> auto key_codec()
{
    static_assert(
        details::has_native_sqlite_support<T>(),
        "Type has no native sqlite support which is required when using identity function");

    return codecs::taged_codec_from<codecs::key_codec_tag, T>();
}

inline auto default_key_codec = key_codec<std::string>();

template <typename E, typename D> auto value_codec(E encoder, D decoder)
{
    return codecs::taged_codec_from<codecs::value_codec_tag, E, D>(encoder, decoder);
}

// use identity function of type T to define a value codec
template <typename T> auto value_codec()
{
    static_assert(
        details::has_native_sqlite_support<T>(),
        "Type has no native sqlite support which is required when using identity function");

    return codecs::taged_codec_from<codecs::value_codec_tag, T>();
}

inline auto default_value_codec = value_codec<std::string>();

enum class operation_mode
{
    c, // default, open db for read/write, creating table if not exists
    r, // open db as read-only
    w, // open db for read/write, but drop table contents first
    n  // create new database (erasing _all_ existing tables!)
};

constexpr const char* default_filename = "";
constexpr const char* default_table = "unnamed";
constexpr operation_mode default_mode = operation_mode::c;
constexpr bool default_auto_commit = false;
constexpr log_level default_log_level = log_level::off;

/**
 * @class configuration
 * @brief Manages database configuration and codec pairs.
 *
 * This template class stores and manages configuration parameters for a database
 * connection, including the codec pair for key and value encoding/decoding.
 *
 * @tparam CODEC_PAIR The codec pair type used for encoding and decoding.
 *
 * configuration is a key component of the database management system, providing
 * a structured way to configure database connections and codec pairs.
 */

template <typename CODEC_PAIR> class configuration
{
  public:
    configuration(CODEC_PAIR codecs)
        : _codecs(codecs)
    {
        static_assert(codecs::is_codec_pair<std::decay_t<CODEC_PAIR>>::value,
                      "CODEC_PAIR must be a specialization of codec_pair<KC, VC>");
    }

    CODEC_PAIR codecs() const
    {
        return _codecs;
    }

    configuration& filename(std::string filename)
    {
        _filename = filename;
        return *this;
    }

    std::string filename() const
    {
        return _filename;
    }

    configuration& table(std::string table)
    {
        _table = table;
        return *this;
    }

    std::string table() const
    {
        return _table;
    }

    configuration& mode(operation_mode mode)
    {
        _mode = mode;
        return *this;
    }

    operation_mode mode() const
    {
        return _mode;
    }

    configuration& auto_commit(bool auto_commit)
    {
        _auto_commit = auto_commit;
        return *this;
    }

    bool auto_commit() const
    {
        return _auto_commit;
    }

    configuration& log_level(log_level log_level)
    {
        _log_level = log_level;
        return *this;
    }

    bw::sqlitemap::log_level log_level() const
    {
        return _log_level;
    }

    configuration& log_impl(logger::log_function log_impl)
    {
        _log_impl = log_impl;
        return *this;
    }

    logger::log_function log_impl() const
    {
        return _log_impl;
    }

    configuration& pragma(const std::string& flag, int value)
    {
        return pragma(flag, std::to_string(value));
    }

    configuration& pragma(const std::string& flag, const std::string& value)
    {
        return pragma("PRAGMA " + flag + " = " + value);
    }

    configuration& pragma(const std::string& statement)
    {
        std::string prefix = "PRAGMA ";
        if (statement.size() < prefix.size() ||
            !std::equal(prefix.begin(), prefix.end(), statement.begin(),
                        [](char a, char b) { return std::tolower(a) == std::tolower(b); }))
        {
            _pragma_statements.push_back(prefix + statement);
            return *this;
        }

        _pragma_statements.push_back(statement);
        return *this;
    }

    const std::vector<std::string>& pragmas() const
    {
        return _pragma_statements;
    }

  private:
    CODEC_PAIR _codecs;
    std::string _filename = default_filename;
    std::string _table = default_table;
    operation_mode _mode = default_mode;
    bool _auto_commit = default_auto_commit;
    bw::sqlitemap::log_level _log_level = default_log_level;
    logger::log_function _log_impl;
    std::vector<std::string> _pragma_statements;
};

template <typename CODEC_PAIR> auto config(CODEC_PAIR codec)
{
    if constexpr (codecs::is_codec_pair<std::decay_t<CODEC_PAIR>>::value)
    {
        return configuration(codec);
    }
    else if constexpr (codecs::is_key_codec<std::decay_t<CODEC_PAIR>>::value)
    {
        return configuration(codecs::codec_pair(codec, default_value_codec));
    }
    else if constexpr (codecs::is_value_codec<std::decay_t<CODEC_PAIR>>::value)
    {
        return configuration(codecs::codec_pair(default_key_codec, codec));
    }
    else
    {
        static_assert(codecs::unknown_codec_tag<CODEC_PAIR>::value, "Unknown CODEC_PAIR type");
    }
}

template <typename KC, typename VC> auto config(KC key_codec, VC value_codec)
{
    return config(codecs::codec_pair(key_codec, value_codec));
}

// Uses key type K and value type V to create configuration from
// both corresponding identity functions
template <typename K, typename V> inline auto config()
{
    return config(key_codec<K>(), value_codec<V>());
}

inline auto config()
{
    return config(default_key_codec, default_value_codec);
}

enum class column_option
{
    key,
    value,
    key_value,
};

/**
 * @class lazy_result
 * @brief Provides lazy evaluation and caching of SQL query results.
 *
 * This template class manages the deferred (lazy) evaluation of results from
 * SQL queries executed on a SQLite database. It decodes keys and values using
 * a provided codec pair and supports different column retrieval modes via the
 * column_option template parameter.
 *
 * The class internally manages the SQLite statement lifecycle and caches results
 * as they are accessed, allowing efficient repeated access to previously evaluated rows.
 *
 * @tparam CODEC_PAIR The codec pair type used for encoding and decoding.
 * @tparam KV The key-value type for the result set.
 * @tparam COL_OPT The column option type (key, value, or key_value).
 *
 * lazy_result is designed for single-pass or sequential access patterns, but it caches
 * all evaluated rows for efficient repeated access. It is not thread-safe.
 */

template <typename CODEC_PAIR, typename KV, column_option COL_OPT> class lazy_result
{
  public:
    using value_type = KV;
    using size_type = size_t;
    using db_key_type = typename CODEC_PAIR::key_out_type;
    using db_mapped_type = typename CODEC_PAIR::value_out_type;

    lazy_result(sqlite3* db, const std::string& query, const configuration<CODEC_PAIR>* config)
        : _db(db)
        , _query(query)
        , _config(config)
        , _stmt(nullptr)
        , _stmt_completed(false)
        , _num_rows(0)
    {
        details::prepare_checked(_db, query, &_stmt);
    }

    lazy_result(value_type&& row, const configuration<CODEC_PAIR>* config)
        : _db(nullptr)
        , _query("")
        , _config(config)
        , _stmt(nullptr)
        , _stmt_completed(true)
        , _num_rows(0)
    {
        cache_item(std::move(row));
    }

    ~lazy_result()
    {
        finalize_stmt();
    }

    const value_type& operator[](size_type index) const
    {
        load_data(index);
        return _data[index];
    }

    size_type evaluated_num_rows() const
    {
        return _num_rows;
    }

    bool result_completed() const
    {
        return _stmt_completed;
    }

    void advance() const
    {
        cache_next();
    }

  private:
    void load_data(size_type to_index) const
    {
        if (_stmt_completed && to_index < _data.size())
            return;

        if (_stmt_completed && to_index >= _num_rows)
            throw std::range_error("Tried to iterate over max num of rows");

        auto need_iteration = [&](size_type i)
        {
            if (i <= to_index)
                return true;

            return !_stmt_completed && i == to_index + 1;
        };

        for (int i = _data.size(); need_iteration(i); i++)
        {
            cache_next();
        }
    }

    //  returns next item, or empty when no item is left
    std::optional<value_type> eval_next() const
    {
        int rc = sqlite3_step(_stmt);
        if (rc == SQLITE_ROW)
        {
            if constexpr (COL_OPT == column_option::key_value)
            {
                auto key = details::column_value<db_key_type>(_stmt, 0);
                auto decoded_key = _config->codecs().key_codec.decode(key);

                auto value = details::column_value<db_mapped_type>(_stmt, 1);
                auto decoded_value = _config->codecs().value_codec.decode(value);

                return value_type{decoded_key, decoded_value};
            }
            else if constexpr (COL_OPT == column_option::key)
            {
                auto key = details::column_value<db_key_type>(_stmt, 0);
                auto decoded_key = _config->codecs().key_codec.decode(key);
                return decoded_key;
            }
            else if constexpr (COL_OPT == column_option::value)
            {
                auto value = details::column_value<db_mapped_type>(_stmt, 0);
                auto decoded_value = _config->codecs().value_codec.decode(value);
                return decoded_value;
            }
            else
            {
                static_assert(COL_OPT == column_option::key || COL_OPT == column_option::value ||
                                  COL_OPT == column_option::key_value,
                              "Unsupported column option");
                throw sqlitemap_error("Unsopported column option");
            }
        }
        else if (rc == SQLITE_DONE)
        {
            finalize_stmt();
        }
        else
        {
            // Finalize the statement in case of error and throw an exception
            auto msg = "Error during SQLite iteration: " + std::string(sqlite3_errmsg(_db));
            finalize_stmt();
            throw sqlitemap_error(msg);
        }
        return std::nullopt;
    }

    void cache_item(value_type&& item) const
    {
        _num_rows++;
        _data.push_back(std::move(item));
    }

    void cache_next() const
    {
        auto row = eval_next();
        if (row.has_value())
        {
            cache_item(std::move(*row));
        }
    }

    void finalize_stmt() const
    {
        if (!_stmt || _stmt_completed)
            return;

        sqlite3_finalize(_stmt);
        _stmt_completed = true;
    }

    sqlite3* _db;
    std::string _query;
    const configuration<CODEC_PAIR>* _config;
    sqlite3_stmt* _stmt;
    mutable bool _stmt_completed;
    mutable size_type _num_rows;
    mutable std::vector<value_type> _data;
};

/**
 * @class sqlitemap_iterator
 * @brief An input iterator for iterating over SQL query results.
 *
 * This template class provides an iterator interface for traversing
 * the results of an SQL query executed on a SQLite database. It supports
 * standard input iterator operations such as incrementing and dereferencing.
 *
 * @tparam CODEC_PAIR The codec pair type used for encoding and decoding.
 * @tparam KV The key-value type for the iterator.
 * @tparam COL_OPT The column option type.
 *
 * The iterator is constructed either from a database query or directly
 * from a row. It uses a lazy evaluation strategy to fetch results as needed.
 * The iterator ensures that it does not advance past the end of the result set.
 *
 * @note This iterator is limited to input operations only, meaning it can
 * only be used for single-pass algorithms that process elements sequentially.
 * It does not support operations such as bidirectional traversal or random access.
 */
template <typename CODEC_PAIR, typename KV, column_option COL_OPT> class sqlitemap_iterator
{
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = KV;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    using size_type = size_t;

    using result_type = lazy_result<CODEC_PAIR, value_type, COL_OPT>;

    sqlitemap_iterator(sqlite3* db, const std::string& query,
                       const configuration<CODEC_PAIR>* config)
        : _lazy_result(std::make_shared<result_type>(db, query, config))
        , _is_end(false)
    {
        advance();
    }

    sqlitemap_iterator(value_type&& row, const configuration<CODEC_PAIR>* config)
        : _lazy_result(std::make_shared<result_type>(std::move(row), config))
        , _is_end(false)
    {
        advance();
    }

    sqlitemap_iterator()
        : _is_end(true)
    {
    }

    sqlitemap_iterator& operator++()
    {
        if (_is_end)
        {
            throw std::out_of_range("Incrementing the iterator past the end is not allowed.");
        }
        advance();
        return *this;
    }

    sqlitemap_iterator operator++(int)
    {
        sqlitemap_iterator temp = *this;
        ++temp;
        return temp;
    }

    sqlitemap_iterator operator+(difference_type n) const
    {
        sqlitemap_iterator temp = *this;
        for (int i = 0; i < n; i++)
            ++temp;
        return temp;
    }

    bool operator==(const sqlitemap_iterator& other) const
    {
        if (_is_end && other._is_end)
            return true;

        return _is_end == other._is_end &&               //
               _current_index == other._current_index && //
               _lazy_result == other._lazy_result;
    }

    bool operator!=(const sqlitemap_iterator& other) const
    {
        return !(*this == other);
    }

    reference operator*() const
    {
        return (*_lazy_result)[*_current_index];
    }

    pointer operator->() const
    {
        return &(**this);
    }

  private:
    void advance()
    {
        if (check_for_end())
            return;

        if (!_lazy_result->result_completed())
        {
            _lazy_result->advance();
        }

        if (check_for_end())
        {
            _current_index = std::nullopt;
        }
        else
        {
            _current_index = _current_index ? ++(*_current_index) : 0;
        }
    }

    bool check_for_end()
    {
        if (!_lazy_result)
        {
            _is_end = true;
        }
        else if (_lazy_result->result_completed())
        {
            bool is_empty = _lazy_result->evaluated_num_rows() == 0;
            bool reached_last_index = _current_index >= _lazy_result->evaluated_num_rows() - 1;
            if (is_empty || reached_last_index)
                _is_end = true;
        }
        return _is_end;
    }

    std::optional<size_type> _current_index;
    std::shared_ptr<result_type> _lazy_result;
    bool _is_end;
};
/**
 * @class const_sqlitemap_iterator
 * @brief Const iterator for iterating over database rows.
 *
 * This template class provides a const iterator for iterating over database rows,
 * which is the const version of sqlitemap_iterator.
 *
 * @tparam CODEC_PAIR The codec pair type used for encoding and decoding.
 * @tparam KV The key-value type used for database operations.
 * @tparam COL_OPT The column option used for database operations.
 *
 * const_sqlitemap_iterator is the const version of sqlitemap_iterator and follows the same
 * principles.
 */
template <typename CODEC_PAIR, typename KV, column_option COL_OPT> class const_sqlitemap_iterator
{
  public:
    using base_iterator = sqlitemap_iterator<CODEC_PAIR, KV, COL_OPT>;
    using iterator_category = typename base_iterator::iterator_category;
    using value_type = typename base_iterator::value_type;
    using difference_type = typename base_iterator::difference_type;
    using pointer = const value_type*;   // Pointer to const value
    using reference = const value_type&; // Reference to const value

    const_sqlitemap_iterator(sqlite3* db, const std::string& query,
                             const configuration<CODEC_PAIR>* config)
        : base_iter_(db, query, config)
    {
    }

    const_sqlitemap_iterator(value_type&& row, const configuration<CODEC_PAIR>* config)
        : base_iter_(std::move(row), config)
    {
    }

    const_sqlitemap_iterator() = default;

    reference operator*() const
    {
        return base_iter_.operator*();
    }
    pointer operator->() const
    {
        return &(operator*());
    }
    const_sqlitemap_iterator& operator++()
    {
        ++base_iter_;
        return *this;
    }

    const_sqlitemap_iterator operator+(difference_type n)
    {
        for (int i = 0; i < n; i++)
            ++base_iter_;
        return *this;
    }

    bool operator==(const const_sqlitemap_iterator& other) const
    {
        return base_iter_ == other.base_iter_;
    }

    bool operator!=(const const_sqlitemap_iterator& other) const
    {
        return base_iter_ != other.base_iter_;
    }

  private:
    base_iterator base_iter_;
};

inline std::vector<std::string> get_tablenames(const std::string& filename)
{
    if (!std::filesystem::exists(filename))
        throw sqlitemap_error("File " + filename + " does not exist");

    sqlite3* db;
    int rc = sqlite3_open(filename.c_str(), &db);
    details::check_ok(rc, "Cannot open database at " + filename, db);

    sqlite3_stmt* stmt = nullptr;
    auto sql = R"(SELECT name FROM sqlite_master WHERE type="table")";
    details::prepare_checked(db, sql, &stmt);

    std::vector<std::string> tables;

    try
    {
        int rc = sqlite3_step(stmt);
        while (rc == SQLITE_ROW)
        {
            auto table = details::column_value<std::string>(stmt, 0);
            tables.push_back(table);

            rc = sqlite3_step(stmt);
        }

        details::check_done(rc);
    }
    catch (const std::exception& e)
    {
        sqlite3_finalize(stmt);
        throw;
    }

    sqlite3_finalize(stmt);
    return tables;
}

template <typename K, typename V> struct sqlitemap_node_type
{
    using value_type = std::optional<std::pair<K, V>>;
    sqlitemap_node_type()
    {
    }

    sqlitemap_node_type(K key, V&& value)
        : kv({key, value})
    {
    }

    K& key()
    {
        return kv->first;
    }

    V& mapped()
    {
        return kv->second;
    }

    operator bool() const
    {
        return kv.operator bool();
    }

  private:
    value_type kv;
};

/**
 * @class sqlitemap
 * @brief High-level C++ interface for SQLite-based key-value maps with codec support.
 *
 * This template class provides a flexible, type-safe, and efficient interface for managing
 * key-value data in a SQLite database. It supports custom codecs for encoding/decoding keys
 * and values, allowing seamless integration with a variety of C++ types and database schemas.
 *
 * The class exposes STL-like map operations, including insertion, lookup, deletion, and iteration.
 * It supports both mutable and const iterators for traversing key-value pairs, keys, or values.
 *
 * Key features:
 *   - Customizable key/value codecs via the CODEC_PAIR template parameter
 *   - STL-style interface (find, insert, erase, begin/end iterators, etc.)
 *   - Efficient lazy evaluation and caching of query results
 *   - Strong type safety for all operations
 *   - Integration with custom configuration objects
 *
 * @tparam CODEC_PAIR The codec pair type used for encoding and decoding keys and values.
 * Defaults to the codec pair from the global config().
 *
 *
 * @note Only single-pass iteration is supported to enable lazy evaluation and caching of query
 * results. As a result, iterators returned by STL-like operations are intended solely for data
 * access; advancing or reusing them in multiple passes is not supported.
 */
template <typename CODEC_PAIR = decltype(config().codecs())> class sqlitemap
{
  public:
    using key_type = typename CODEC_PAIR::key_in_type;
    using mapped_type = typename CODEC_PAIR::value_in_type; // type that corresponds to value in db
    using value_type = std::pair<key_type, mapped_type>;    // important!  tuple of (key,value)
    using node_type = sqlitemap_node_type<key_type, mapped_type>;
    using db_key_type = typename CODEC_PAIR::key_out_type;
    using db_mapped_type = typename CODEC_PAIR::value_out_type;
    using size_type = size_t;

    using iterator = sqlitemap_iterator<CODEC_PAIR, value_type, column_option::key_value>;
    using const_iterator =
        const_sqlitemap_iterator<CODEC_PAIR, value_type, column_option::key_value>;

    using key_iterator = sqlitemap_iterator<CODEC_PAIR, key_type, column_option::key>;
    using const_key_iterator = const_sqlitemap_iterator<CODEC_PAIR, key_type, column_option::key>;

    using value_iterator = sqlitemap_iterator<CODEC_PAIR, mapped_type, column_option::value>;
    using const_value_iterator =
        const_sqlitemap_iterator<CODEC_PAIR, mapped_type, column_option::value>;

    struct sqlitemap_insert_return_type
    {
        iterator position; // Iterator to the inserted element or the existing one
        bool inserted;     // True if insertion happened, false if key already exists
        node_type node;    // The node, if insertion didn't happen (ownership is returned)
    };
    using insert_return_type = sqlitemap_insert_return_type;

    template <typename K = key_type, typename V = mapped_type> class value_ref
    {
      public:
        value_ref(sqlitemap<CODEC_PAIR>* map, const K& key, V value)
            : _map(map)
            , _key(key)
            , _val(value)
        {
        }

        value_ref& operator=(const value_ref& other) = delete;

        value_ref& operator=(V value)
        {
            _map->set(_key, value);
            _val = value;
            return *this;
        }

        operator V() const
        {
            return _val;
        }

        bool operator==(const V& other_value)
        {
            return _val == other_value;
        }

      private:
        sqlitemap<CODEC_PAIR>* _map;
        K _key;
        V _val;

        friend std::ostream& operator<<(std::ostream& os, const value_ref& ref)
        {
            os << ref._val;
            return os;
        }
    };

    sqlitemap(std::string filename = default_filename, std::string table = default_table,
              operation_mode mode = default_mode, bool auto_commit = default_auto_commit,
              log_level log_level = default_log_level)
        : sqlitemap(bw::sqlitemap::config() // use default configuration
                        .filename(filename)
                        .table(table)
                        .mode(mode)
                        .auto_commit(auto_commit)
                        .log_level(log_level))
    {
    }

    sqlitemap(configuration<CODEC_PAIR> config)
        : _config(std::move(config))
    {
        log().set_level(_config.log_level());
        if (_config.log_impl())
            log().register_log_impl(_config.log_impl());

        namespace fs = std::filesystem;
        try
        {
            if (_config.filename().empty())
            {
                auto dir = fs::temp_directory_path();
                auto file = details::generate_temp_filename();
                _config.filename((dir / file).string());
                _in_temp = true;
            }

            if (_config.mode() == operation_mode::n && fs::exists(_config.filename()))
                fs::remove(_config.filename());
        }
        catch (std::exception& ex)
        {
            throw sqlitemap_error(ex.what());
        }

        auto dir = fs::path(_config.filename()).parent_path();
        if (dir.is_relative())
            dir = fs::current_path() / dir;

        if (!in_memory() && !fs::exists(dir))
            throw sqlitemap_error("The directory does not exist: " + dir.string());

        log().debug("sqlitemap - file: '" + _config.filename() + "' table: '" + _config.table() +
                    "' sqlite3_libversion: " + sqlite3_libversion());

        connect();
    }

    ~sqlitemap()
    {
        try
        {
            close();
        }
        catch (std::exception& ex)
        {
            log().error(std::string("Descruction of sqlitemap failed. Error: ") + ex.what());
        }
    }

    void open_database(const std::string& file)
    {
        try
        {
            int rc = SQLITE_ERROR;
            if (is_read_only())
            {
                // open database but do not create one if missing
                rc = sqlite3_open_v2(file.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
            }
            else
            {
                // open or create the database
                rc = sqlite3_open(file.c_str(), &db);
            }

            details::check_ok(rc, "Cannot open database", db);
            log().debug("Database '" + config().filename() + "' opened successfully!");
        }
        catch (const std::exception& e)
        {
            sqlite3_close(db);
            throw;
        }
    }

    // Connects to the underlying database and initializes the table if required. Throws exception
    // in error cases.
    void connect()
    {
        using namespace codecs;

        try
        {
            open_database(config().filename());

            if (is_read_only())
            {
                std::vector<std::string> tables = get_tablenames(config().filename());
                if (std::find(tables.begin(), tables.end(), config().table()) == tables.end())
                {
                    std::string error_message = "Refusing to create a new table '" +
                                                config().table() + "' in read-only DB mode";
                    throw sqlitemap_error(error_message);
                }
            }

            // execute pragma statements
            for (const auto& pragma_statement : config().pragmas())
            {
                details::exec_checked(db, pragma_statement);
            }

            // create table if missing
            auto key_type = codecs::to_string(sqlite_storage_class_from_type<db_key_type>());
            auto value_type = codecs::to_string(sqlite_storage_class_from_type<db_mapped_type>());
            auto create_table_sql = sql("CREATE TABLE IF NOT EXISTS :table (key " + key_type +
                                        " PRIMARY KEY, value " + value_type + ")");

            details::exec_checked(db, create_table_sql);
            commit();
            log().debug("Table '" + config().table() + "' created successfully");

            if (config().mode() == operation_mode::w)
            {
                clear();
            }
        }
        catch (const std::exception& e)
        {
            sqlite3_close(db);
            throw;
        }
    }

    // Get the underlying SQLite connection object, this is used for advanced users
    // who want to use the connection directly to execute custom queries
    sqlite3* get_connection() const
    {
        return db;
    }

    void set(const key_type& key, const mapped_type& value)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to write to read-only sqlitemap");

        sqlite3_stmt* stmt = nullptr;
        auto set_sql = sql("REPLACE INTO :table (key, value) VALUES (?,?)");
        details::prepare_checked(db, set_sql, &stmt);

        try
        {
            auto encoded_key = _config.codecs().key_codec.encode(key);
            details::bind_param_checked(stmt, 1, encoded_key, "Failed to bind key", db);

            auto encoded_value = _config.codecs().value_codec.encode(value);
            details::bind_param_checked(stmt, 2, encoded_value, "Failed to bind value", db);

            // sqlite auto commits changes when _no_ transactions was started by user
            if (!config().auto_commit())
                begin_transaction();

            details::check_done(sqlite3_step(stmt), db);
            sqlite3_finalize(stmt);
        }
        catch (const std::exception& e)
        {
            // clean up and rethrow the exception
            sqlite3_finalize(stmt);
            throw;
        }
    }

    // get value associated with key. Throws a sqliteman_error when key does not exist
    // also cf. try_get for a not throwing alternative
    mapped_type get(const key_type& key) const
    {
        try
        {
            return try_get(key).value();
        }
        catch (const std::bad_optional_access& e)
        {
            throw sqlitemap_error("Key '" + details::as_string_or(key) + "' not found in database");
        }
    }

    // get optional value associated with key.
    std::optional<mapped_type> try_get(const key_type& key) const
    {
        sqlite3_stmt* stmt = nullptr;
        auto get_sql = sql("SELECT value FROM :table WHERE key = ?");
        details::prepare_checked(db, get_sql, &stmt);

        try
        {
            auto encoded_key = _config.codecs().key_codec.encode(key);
            details::bind_param_checked(stmt, 1, encoded_key, "Failed to bind key", db);

            int rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE)
            {
                sqlite3_finalize(stmt);
                return std::nullopt;
            }
            details::require_return_code(rc, SQLITE_ROW, "Failed to execute statement", db);

            auto value = details::column_value<db_mapped_type>(stmt, 0);
            sqlite3_finalize(stmt);

            auto decoded_value = _config.codecs().value_codec.decode(value);
            return decoded_value;
        }
        catch (const std::exception& e)
        {
            // clean up and rethrow the exception
            sqlite3_finalize(stmt);
            throw;
        }
    }

    value_ref<key_type, mapped_type> at(const key_type& key)
    {
        return value_ref(this, key, get(key));
    }

    value_ref<key_type, mapped_type> operator[](const key_type& key)
    {
        try
        {
            return at(key);
        }
        catch (std::exception& ex)
        {
            auto default_value = mapped_type();
            set(key, default_value);
            return value_ref(this, key, default_value);
        }
    }

    void del(const key_type& key)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to delete from read-only sqlitemap");

        sqlite3_stmt* stmt = nullptr;
        auto del_sql = sql("DELETE FROM :table WHERE key = ?");
        details::prepare_checked(db, del_sql, &stmt);

        try
        {
            auto encoded_key = _config.codecs().key_codec.encode(key);
            details::bind_param_checked(stmt, 1, encoded_key, "Failed to bind key", db);

            // sqlite auto commits changes when _no_ transactions was started by user
            if (!config().auto_commit())
                begin_transaction();

            details::check_done(sqlite3_step(stmt), db);
            sqlite3_finalize(stmt);
        }
        catch (const std::exception& e)
        {
            // Clean up and rethrow the exception
            sqlite3_finalize(stmt);
            throw;
        }
    }

    size_t size() const
    {
        // `select count (*)` is super slow in sqlite but ok for now?

        auto count_callback = [](void* count_ptr, int argc, char** argv, char** col_name)
        {
            int* row_count = (int*)count_ptr;
            *row_count = atoi(argv[0]); // The result of COUNT(*) is in the first column
            return 0;
        };

        int row_count = 0;
        auto count_sql = sql("SELECT COUNT(*) FROM :table");
        details::exec_checked(db, count_sql, count_callback, &row_count);

        return row_count;
    }

    bool empty() const
    {
        return size() == 0;
    }

    std::pair<iterator, bool> insert(const value_type& kv)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to insert into read-only sqlitemap");

        auto mapped = try_get(kv.first);
        if (mapped)
            return {find(kv.first), false};

        set(kv.first, kv.second);
        return {find(kv.first), true};
    }

    std::pair<iterator, bool> insert(value_type&& kv)
    {
        return insert(kv);
    }

    insert_return_type insert(node_type&& node)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to insert into read-only sqlitemap");

        if (!node)
            return {end(), false, std::move(node)};

        auto result = insert({node.key(), node.mapped()});
        return {result.first, result.second, result.second ? node_type() : std::move(node)};
    }

    void insert(std::initializer_list<std::pair<const key_type, mapped_type>> list)
    {
        return insert(list.begin(), list.end());
    }

    template <typename _InputIterator> void insert(_InputIterator __first, _InputIterator __last)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to insert into read-only sqlitemap");

        for (; __first != __last; ++__first)
        {
            if (!contains(__first->first))
                set(__first->first, __first->second);
        }
    }

    template <typename Object>
    std::pair<iterator, bool> insert_or_assign(const key_type& key, Object&& value)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to write to read-only sqlitemap");

        auto result = try_emplace(key, std::forward<Object>(value));
        if (result.second)
            return result;

        set(key, std::forward<Object>(value));
        return {find(key), false};
    }

    template <typename Object>
    std::pair<iterator, bool> insert_or_assign(key_type&& key, Object&& value)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to write to read-only sqlitemap");

        auto result = try_emplace(std::move(key), std::forward<Object>(value));
        if (result.second)
            return result;

        set(std::move(key), std::forward<Object>(value));
        return {find(key), false};
    }

    template <typename... Args> std::pair<iterator, bool> emplace(Args&&... args)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to write to read-only sqlitemap");

        return insert(std::move(value_type(std::forward<Args>(args)...)));
    }

    // As sqlitemap has unordered keys, the hint will be ignored so that this methods behavior is
    // equal to calling emplace method directly.
    template <typename... Args> iterator emplace_hint(const_iterator hint, Args&&... args)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to write to read-only sqlitemap");

        return emplace(std::forward<Args>(args)...).first;
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(const key_type& key, Args&&... args)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to write to read-only sqlitemap");

        if (contains(key))
            return {find(key), false};

        return emplace(std::piecewise_construct,
                       std::forward_as_tuple(key), //
                       std::forward_as_tuple(std::forward<Args>(args)...));
    }

    template <typename... Args>
    std::pair<iterator, bool> try_emplace(key_type&& key, Args&&... args)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to write to read-only sqlitemap");

        if (contains(key))
            return {find(key), false};

        return emplace(std::piecewise_construct,
                       std::forward_as_tuple(std::move(key)), //
                       std::forward_as_tuple(std::forward<Args>(args)...));
    }

    template <typename... Args>
    iterator try_emplace(const_iterator hint, const key_type& key, Args&&... args)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to write to read-only sqlitemap");

        if (contains(key))
            return find(key);

        return emplace_hint(hint, std::piecewise_construct,
                            std::forward_as_tuple(key), //
                            std::forward_as_tuple(std::forward<Args>(args)...));
    }

    template <typename... Args>
    iterator try_emplace(const_iterator hint, key_type&& key, Args&&... args)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to write to read-only sqlitemap");

        if (contains(key))
            return find(key);

        return emplace_hint(hint, std::piecewise_construct,
                            std::forward_as_tuple(std::move(key)), //
                            std::forward_as_tuple(std::forward<Args>(args)...));
    }

    iterator find(const key_type& key)
    {
        auto value = try_get(key);
        if (value)
            return iterator(std::make_pair(key, std::move(*value)), &_config);

        return end();
    }

    const_iterator find(const key_type& key) const
    {
        auto value = try_get(key);
        if (value)
            return const_iterator(std::make_pair(key, std::move(*value)), &_config);

        return cend();
    }

    size_type count(const key_type& key) const
    {
        sqlite3_stmt* stmt = nullptr;
        auto contains_sql = sql("SELECT EXISTS(SELECT 1 FROM :table WHERE key = ?)");
        details::prepare_checked(db, contains_sql, &stmt);

        try
        {
            auto encoded_key = _config.codecs().key_codec.encode(key);
            details::bind_param_checked(stmt, 1, encoded_key, "Failed to bind key", db);

            int rc = sqlite3_step(stmt);
            details::require_return_code(rc, SQLITE_ROW, "Failed to execute statement", db);

            auto contains = details::column_value<int>(stmt, 0);
            sqlite3_finalize(stmt);

            return contains;
        }
        catch (const std::exception& e)
        {
            // clean up and rethrow the exception
            sqlite3_finalize(stmt);
            throw;
        }
    }

    bool contains(const key_type& key) const
    {
        return count(key);
    }

    void clear()
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to clear read-only sqlitemap");

        commit();

        auto clear_sql = sql("DELETE FROM :table");
        details::exec_checked(db, clear_sql);
        commit();
    }

    // Erases entry for a given key. Returns 0 when key does not exists, 1 otherwise
    size_type erase(const key_type& key)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to erase from read-only sqlitemap");

        if (!contains(key))
            return 0;

        del(key);
        return 1;
    }

    // Erases entry when applies Predicate is true. Returns number of erased entries
    template <typename Predicate> size_type erase_if(Predicate predicate)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to erase from read-only sqlitemap");

        size_t num_erased_elements = 0;
        for (auto it = begin(); it != end(); ++it)
        {
            if (predicate(*it))
                num_erased_elements += erase(it->first);
        }

        return num_erased_elements;
    }

    node_type extract(const key_type& key)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to extract from read-only sqlitemap");

        auto value = try_get(key);
        if (value)
        {
            del(key);
            return node_type(key, std::move(*value));
        }

        return node_type();
    }

    node_type extract(const_iterator position)
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to extract from read-only sqlitemap");

        if (position != cend())
        {
            return extract(position->first);
        }

        return node_type();
    }

    std::pair<iterator, iterator> equal_range(const key_type& key)
    {
        auto it = find(key);
        return {it, it};
    }

    std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const
    {
        auto it = find(key);
        return {it, it};
    }

    void begin_transaction()
    {
        // details::exec_checked(db, "BEGIN TRANSACTION");
        int rc = sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
    }

    void commit()
    {
        // details::exec_checked(db, "COMMIT");
        int rc = sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
    }

    void rollback()
    {
        // details::exec_checked(db, "ROLLBACK");
        int rc = sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    }

    void close()
    {
        if (config().auto_commit())
            commit();

        // Close the database connection
        sqlite3_close(db);
        log().debug("Database closed");

        if (in_temp())
        {
            try
            {
                std::filesystem::remove(_config.filename());
                log().debug("Database file '" + config().filename() + "' removed");
            }
            catch (std::exception& ex)
            {
                /**/
            }
        }
    }

    // Delete the underlying database file. Use with care.
    void terminate()
    {
        if (is_read_only())
            throw sqlitemap_error("Refusing to terminate read-only sqlitemap");

        close();

        if (config().filename() == ":memory:")
            return;

        log().debug("Deleting " + config().filename());

        try
        {
            std::string file = _config.filename();
            if (std::filesystem::exists(file))
                std::filesystem::remove(file);
        }
        catch (const std::exception& e)
        {
            log().error("Failed to delete " + config().filename());
        }
    }

    const configuration<CODEC_PAIR>& config() const
    {
        return _config;
    }

    std::string to_string() const
    {
        return "sqlitemap(" + _config.filename() + ")";
    }

    bool in_memory() const
    {
        return _config.filename() == ":memory:";
    }

    bool in_temp() const
    {
        return _in_temp;
    }

    bool is_read_only() const
    {
        return _config.mode() == operation_mode::r;
    }

    iterator begin()
    {
        std::string query = sql("SELECT key, value FROM :table");
        return iterator(db, query, &_config);
    }

    iterator end()
    {
        return iterator();
    }

    const_iterator begin() const
    {
        std::string query = sql("SELECT key, value FROM :table");
        return const_iterator(db, query, &_config);
    }

    const_iterator end() const
    {
        return const_iterator();
    }

    const_iterator cbegin() const
    {
        return begin();
    }

    const_iterator cend() const
    {
        return end();
    }

    iterator rbegin()
    {
        std::string query = sql("SELECT key, value FROM :table ORDER BY ROWID DESC");
        return iterator(db, query, &_config);
    }

    iterator rend()
    {
        return iterator();
    }

    const_iterator rbegin() const
    {
        std::string query = sql("SELECT key, value FROM :table ORDER BY ROWID DESC");
        return const_iterator(db, query, &_config);
    }

    const_iterator rend() const
    {
        return const_iterator();
    }

    const_iterator crbegin() const
    {
        return rbegin();
    }

    const_iterator crend() const
    {
        return rend();
    }

    key_iterator keys_begin()
    {
        std::string query = sql("SELECT key FROM :table");
        return key_iterator(db, query, &_config);
    }

    key_iterator keys_end()
    {
        return key_iterator();
    }

    key_iterator keys_rbegin()
    {
        std::string query = sql("SELECT key FROM :table ORDER BY ROWID DESC");
        return key_iterator(db, query, &_config);
    }

    key_iterator keys_rend()
    {
        return key_iterator();
    }

    const_key_iterator keys_cbegin()
    {
        std::string query = sql("SELECT key FROM :table");
        return const_key_iterator(db, query, &_config);
    }

    const_key_iterator keys_cend()
    {
        return const_key_iterator();
    }

    const_key_iterator keys_crbegin()
    {
        std::string query = sql("SELECT key FROM :table ORDER BY ROWID DESC");
        return const_key_iterator(db, query, &_config);
    }

    const_key_iterator keys_crend()
    {
        return const_key_iterator();
    }
    value_iterator values_begin()
    {
        std::string query = sql("SELECT value FROM :table");
        return value_iterator(db, query, &_config);
    }

    value_iterator values_end()
    {
        return value_iterator();
    }

    value_iterator values_rbegin()
    {
        std::string query = sql("SELECT value FROM :table ORDER BY ROWID DESC");
        return value_iterator(db, query, &_config);
    }

    value_iterator values_rend()
    {
        return value_iterator();
    }

    const_value_iterator values_cbegin()
    {
        std::string query = sql("SELECT value FROM :table");
        return const_value_iterator(db, query, &_config);
    }

    const_value_iterator values_cend()
    {
        return const_value_iterator();
    }

    const_value_iterator values_crbegin()
    {
        std::string query = sql("SELECT value FROM :table ORDER BY ROWID DESC");
        return const_value_iterator(db, query, &_config);
    }

    const_value_iterator values_crend()
    {
        return const_value_iterator();
    }

    // configures a sql statement to use correct table by replacing :table with that one from
    // configuration and put it in double quotes.
    // e.g. 'select * from :table' => 'select * from "unnamed"'
    std::string sql(const std::string& sql) const
    {
        const std::string placeholder = ":table";
        const std::string replacement = "\"" + _config.table() + "\"";
        std::string output = sql;

        size_t pos = 0;
        while ((pos = output.find(placeholder, pos)) != std::string::npos)
        {
            output.replace(pos, placeholder.length(), replacement);
            pos += replacement.length();
        }

        return output;
    }

    logger& log()
    {
        return _logger;
    }

  private:
    sqlite3* db;
    configuration<CODEC_PAIR> _config;
    bool _in_temp = false;
    logger _logger;
};

} // namespace bw::sqlitemap