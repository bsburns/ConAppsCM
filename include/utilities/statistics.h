#pragma once
/*------------------------------------------------------------------
 * Statistics Header File
 *
 * Defines functions and classes for monitoring and managing system processes.
 *
 * April 2026, Barry S. Burns (B2)
 *
 *------------------------------------------------------------------
 */
#include <vector>

// CRTP Implementation for stats
class StatisticsBase {
private:



public:
	std::string Name; // Stat Name
    std::string LongName; // Long Stat Name
	StatisticsBase *Parent = nullptr;
	std::vector<StatisticsBase *> ChildStats;


    virtual std::string GetColumnHeader() = 0;
    virtual std::string GetColumnData() = 0;
    virtual std::string ToString() = 0;

    virtual std::string GetChildColumnHeader() = 0;
    virtual std::string GetChildColumnData() = 0;
    virtual std::string ToStringChild() = 0;

    void clear() {
        for (auto child : ChildStats) {
            child->clear();
        }
        ChildStats.clear();
    }

    std::string FormatChildren(std::string indent = "") {
        std::string str = "\n" +indent + LongName;
        indent += "\t";
        for (auto child : ChildStats) {
            str += child->FormatChildren(indent);
        }
        return str;
    }
    
    virtual ~StatisticsBase() = default;
};

template <typename Derived>
class StatisticsBaseCRTP : public StatisticsBase {
public:
    StatisticsBaseCRTP() {}
    StatisticsBaseCRTP(std::string name, StatisticsBase* parent = nullptr) {
        Name = name;
        Parent = parent;
        if (parent != nullptr) {
            parent->ChildStats.push_back(this);
            LongName = parent->LongName + "." + Name;
        }
        else {
            LongName = name;
        }
    }
    std::string ToString() override {
        return static_cast<Derived*>(this)->ToString();
    }
    std::string GetColumnHeader() override {
        return static_cast<Derived*>(this)->GetColumnHeader();
    }
    std::string GetColumnData() override {
        return static_cast<Derived*>(this)->GetColumnData();
    }

    std::string GetChildColumnHeader() override {
        std::string str = "";
        for (auto& child : ChildStats) {
            str += child->GetColumnHeader();
        }
        return str;
	}
    std::string GetChildColumnData() override {
        std::string str = "";
        for (auto& child : ChildStats) {
            str += child->GetColumnData();
        }
        return str;
    }
    std::string ToStringChild() override {
        std::string str = "";
        for (auto& child : ChildStats) {
            str += "\n\t" + child->ToString();
        }
        return str;
	}
};

class StatisticsNone : public StatisticsBaseCRTP<StatisticsNone> {
    using Base = StatisticsBaseCRTP<StatisticsNone>;
public:

    StatisticsNone(std::string name, StatisticsBase* parent = nullptr) 
        : StatisticsBaseCRTP<StatisticsNone>(name, parent) {
        //Base::Name = name;
        //Base::Parent = parent;
        //if (parent != nullptr) {
        //    parent->ChildStats.push_back(this);
        //    Base::LongName = parent->LongName + "." + Base::Name;
        //}
        //else {
        //    Base::LongName = name;
        //}
    }

   
    std::string GetColumnHeader() override {
        return Base::GetChildColumnHeader();
    }
    
    std::string GetColumnData() override {
        return Base::GetChildColumnData();
    }

    std::string ToString() {
		return Base::ToStringChild();
    }

};

template <typename T>
class StatisticsBasic : public StatisticsBaseCRTP<StatisticsBasic<T>> {
    using Base = StatisticsBaseCRTP<StatisticsBasic<T>>;
protected:
    size_t count_ = 0;
    T sum_ = T{};
    T min_ = std::numeric_limits<T>::max();
    T max_ = std::numeric_limits<T>::lowest();
public:
    StatisticsBasic() {}
    StatisticsBasic(std::string name, StatisticsBase* parent = nullptr) 
        : StatisticsBaseCRTP<StatisticsBasic>(name, parent) {
        //Base::Name = name;
        //Base::Parent = parent;
        //if (parent != nullptr) {
        //    parent->ChildStats.push_back(this);
        //    Base::LongName = parent->LongName + "." + Base::Name;
        //}
        //else {
        //    Base::LongName = name;
        //}
    }
    void addValue(const T& value) {
        static_assert(std::is_arithmetic<T>::value, "T must be numeric");

        ++count_;
        sum_ += value;
        if (value < min_) min_ = value;
        if (value > max_) max_ = value;

        // Allow derived class to extend behavior
        //static_cast<Derived*>(this)->onValueAdded(value);
    }

    // Getters
    size_t count() const { return count_; }
    T sum() const { return sum_; }
    T mean() const {
        if (count_ == 0) return 0;
        return sum_ / static_cast<T>(count_);
    }
    T min() const {
        //if (count_ == 0) throw std::runtime_error("No data to compute min");
        return min_;
    }
    T max() const {
        //if (count_ == 0) throw std::runtime_error("No data to compute max");
        return max_;
    }
    
    std::string GetColumnHeader() override {
        std::string str;
        str  = Base::LongName + "." + "sum, ";
        str += Base::LongName + "." + "mean, ";
        str += Base::LongName + "." + "min, ";
        str += Base::LongName + "." + "max, ";
        str += Base::LongName + "." + "cnt, ";
        str += Base::GetChildColumnHeader();
        return str;
    }
    
    std::string GetColumnData() override {
        std::string str;
        str  = std::to_string(sum_) + ", ";
        str += std::to_string(mean()) + ", ";
        str += std::to_string(min_) + ", ";
        str += std::to_string(max_) + ", ";
        str += std::to_string(count_) + ", ";
        str += Base::GetChildColumnData();
        return str;
    }

    std::string ToString() {
        std::string str = "LongName=" + Base::LongName;
        str += "\n\tsum="+ std::to_string(sum_);
        str += " mean=" + std::to_string(mean());
        str += " min=" + std::to_string(min_);
        str += " max=" + std::to_string(max_);
        str += " cnt=" + std::to_string(count_);
        str += Base::ToStringChild();
        return str;
    }
};
