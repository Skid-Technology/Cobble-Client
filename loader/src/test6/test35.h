#pragma once

#include <map>
#include <string>
#include <vector>





namespace json {

class Value {
public:
    enum Type { Null, Bool, Number, String };

    Value() : type_(Null), bool_(false), number_(0) {}
    Value(bool b) : type_(Bool), bool_(b), number_(0) {}
    Value(int n) : type_(Number), bool_(false), number_(n) {}
    Value(double n) : type_(Number), bool_(false), number_(n) {}
    Value(const std::string& s) : type_(String), bool_(false), number_(0), string_(s) {}
    Value(const char* s) : type_(String), bool_(false), number_(0), string_(s) {}

    Type type() const { return type_; }
    bool is_null() const { return type_ == Null; }
    bool is_bool() const { return type_ == Bool; }
    bool is_number() const { return type_ == Number; }
    bool is_string() const { return type_ == String; }

    bool as_bool() const { return bool_; }
    int as_int() const { return static_cast<int>(number_); }
    double as_double() const { return number_; }
    const std::string& as_string() const { return string_; }

private:
    Type type_;
    bool bool_;
    double number_;
    std::string string_;
};

class Object {
public:
    bool parse(const std::string& json) {
        data_.clear();
        size_t pos = 0;
        return parseObject(json, pos);
    }

    bool has(const std::string& key) const { return data_.find(key) != data_.end(); }

    const Value& get(const std::string& key) const {
        static Value null;
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : null;
    }

    bool get_bool(const std::string& key, bool def = false) const {
        auto it = data_.find(key);
        if (it != data_.end() && it->second.is_bool())
            return it->second.as_bool();
        return def;
    }

    int get_int(const std::string& key, int def = 0) const {
        auto it = data_.find(key);
        if (it != data_.end() && it->second.is_number())
            return it->second.as_int();
        return def;
    }

    std::string get_string(const std::string& key, const std::string& def = "") const {
        auto it = data_.find(key);
        if (it != data_.end() && it->second.is_string())
            return it->second.as_string();
        return def;
    }

private:
    std::map<std::string, Value> data_;

    void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.length() &&
               (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
            pos++;
        }
    }

    bool parseObject(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.length() || json[pos] != '{')
            return false;
        pos++;

        while (pos < json.length()) {
            skipWhitespace(json, pos);
            if (json[pos] == '}') {
                pos++;
                return true;
            }

            std::string key;
            if (!parseString(json, pos, key))
                return false;

            skipWhitespace(json, pos);
            if (pos >= json.length() || json[pos] != ':')
                return false;
            pos++;

            Value val;
            if (!parseValue(json, pos, val))
                return false;
            data_[key] = val;

            skipWhitespace(json, pos);
            if (json[pos] == ',')
                pos++;
        }
        return false;
    }

    bool parseString(const std::string& json, size_t& pos, std::string& out) {
        skipWhitespace(json, pos);
        if (pos >= json.length() || json[pos] != '"')
            return false;
        pos++;

        out.clear();
        while (pos < json.length() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.length()) {
                pos++;
                switch (json[pos]) {
                    case '"':
                        out += '"';
                        break;
                    case '\\':
                        out += '\\';
                        break;
                    case 'n':
                        out += '\n';
                        break;
                    case 't':
                        out += '\t';
                        break;
                    case 'r':
                        out += '\r';
                        break;
                    default:
                        out += json[pos];
                        break;
                }
            } else {
                out += json[pos];
            }
            pos++;
        }

        if (pos >= json.length())
            return false;
        pos++;
        return true;
    }

    bool parseValue(const std::string& json, size_t& pos, Value& out) {
        skipWhitespace(json, pos);
        if (pos >= json.length())
            return false;

        char c = json[pos];

        if (c == '"') {
            std::string s;
            if (!parseString(json, pos, s))
                return false;
            out = Value(s);
            return true;
        }

        if (c == 't' && json.substr(pos, 4) == "true") {
            pos += 4;
            out = Value(true);
            return true;
        }

        if (c == 'f' && json.substr(pos, 5) == "false") {
            pos += 5;
            out = Value(false);
            return true;
        }

        if (c == 'n' && json.substr(pos, 4) == "null") {
            pos += 4;
            out = Value();
            return true;
        }

        if (c == '-' || (c >= '0' && c <= '9')) {
            size_t start = pos;
            if (json[pos] == '-')
                pos++;
            while (pos < json.length() && json[pos] >= '0' && json[pos] <= '9')
                pos++;
            if (pos < json.length() && json[pos] == '.') {
                pos++;
                while (pos < json.length() && json[pos] >= '0' && json[pos] <= '9')
                    pos++;
            }
            std::string numStr = json.substr(start, pos - start);
            out = Value(std::stod(numStr));
            return true;
        }

        return false;
    }
};

}
