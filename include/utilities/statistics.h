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
#include <chrono>
#include <iostream>

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


template <typename T>
class StatisticsRTM : public StatisticsBaseCRTP<StatisticsRTM<T>> {
    using Base = StatisticsBaseCRTP<StatisticsRTM<T>>;
protected:
	double period_secs_ = 1.0;
    size_t count_ = 0;
    T sum_ = T{};
    T min_ = std::numeric_limits<T>::max();
    T max_ = std::numeric_limits<T>::lowest();
	std::chrono::steady_clock::time_point last_sample_time_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point last_period_time_ = std::chrono::steady_clock::now();
    double instaneous_cnt_rate_ = 0.0;
    double instaneous_unit_rate_ = 0.0;
	double period_cnt_rate_ = 0.0;
	double period_unit_rate_ = 0.0;
    double prev_period_cnt_rate_ = 0.0;
    double prev_period_unit_rate_ = 0.0;
    size_t prev_period_cnt_ = 0;
	T prev_period_sum_ = T{};
    bool debug_ = false;
public:
    StatisticsRTM() {}
    StatisticsRTM(std::string name, StatisticsBase* parent = nullptr, double period_secs = 1, bool debug = false)
        : StatisticsBaseCRTP<StatisticsRTM>(name, parent) 
		, period_secs_(period_secs)
        , debug_(debug)
    {
    }
    void addValue(const T& value) {
        static_assert(std::is_arithmetic<T>::value, "T must be numeric");
        auto curr_time = std::chrono::steady_clock::now();
		std::chrono::duration<double> elapsed = curr_time - last_sample_time_;
        last_sample_time_ = curr_time;
        std::chrono::duration<double> elapsed_period = curr_time - last_period_time_;
        instaneous_cnt_rate_ = 1.0 / elapsed.count();
		instaneous_unit_rate_ = value / elapsed.count();
        if (debug_) {
            std::cout << "\n\nStats::"
                << Base::LongName
                //<< " curr_time=" << curr_time
                << " elapsed_time=" << elapsed.count()
                << " elapsed_period_time=" << elapsed_period.count()
                << " icps=" << instaneous_cnt_rate_
                << " iups=" << instaneous_unit_rate_
                << "\n";
        }
        if (elapsed_period.count() >= period_secs_) {
            period_cnt_rate_ = static_cast<double>(count_ - prev_period_cnt_) / elapsed_period.count();
            period_unit_rate_ = static_cast<double>(sum_ - prev_period_sum_) / elapsed_period.count();
			prev_period_cnt_rate_ = period_cnt_rate_;
			prev_period_unit_rate_ = period_unit_rate_;
            prev_period_cnt_ = count_;
            prev_period_sum_ = sum_;
            last_period_time_ = curr_time;
		} else {
            auto cnt_rate = static_cast<double>(count_ - prev_period_cnt_) / elapsed_period.count();
            auto unit_rate = static_cast<double>(sum_ - prev_period_sum_) / elapsed_period.count();
			auto alpha = elapsed_period.count() / period_secs_;
            period_cnt_rate_ = prev_period_cnt_rate_*(1 - alpha) + cnt_rate*alpha;
            period_unit_rate_ = prev_period_unit_rate_*(1-alpha) + unit_rate*alpha;
		}
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
	double instaneousCountRate() const { return instaneous_cnt_rate_; }
	double instaneousUnitRate() const { return instaneous_unit_rate_; }
	double periodCountRate() const { return period_cnt_rate_; }
	double periodUnitRate() const { return period_unit_rate_; }

    std::string GetColumnHeader() override {
        std::string str;
        str = Base::LongName + "." + "sum, ";
        str += Base::LongName + "." + "mean, ";
        str += Base::LongName + "." + "min, ";
        str += Base::LongName + "." + "max, ";
        str += Base::LongName + "." + "cnt, ";
        str += Base::LongName + "." + "icps";
        str += Base::LongName + "." + "pcps";
        str += Base::LongName + "." + "iups";
        str += Base::LongName + "." + "pups";
        str += Base::GetChildColumnHeader();
        return str;
    }

    std::string GetColumnData() override {
        std::string str;
        str = std::to_string(sum_) + ", ";
        str += std::to_string(mean()) + ", ";
        str += std::to_string(min_) + ", ";
        str += std::to_string(max_) + ", ";
        str += std::to_string(count_) + ", ";
        str += std::to_string(instaneous_cnt_rate_) + ", ";
        str += std::to_string(period_cnt_rate_) + ", ";
        str += std::to_string(instaneous_unit_rate_) + ", ";
        str += std::to_string(period_unit_rate_) + ", ";

        str += Base::GetChildColumnData();
        return str;
    }

    std::string ToString() {
        std::string str = "LongName=" + Base::LongName;
        str += "\n\tsum=" + std::to_string(sum_);
        str += " mean=" + std::to_string(mean());
        str += " min=" + std::to_string(min_);
        str += " max=" + std::to_string(max_);
        str += " cnt=" + std::to_string(count_);
        str += " icps=" + std::to_string(instaneous_cnt_rate_);
        str += " pcps=" + std::to_string(period_cnt_rate_);
        str += " iups=" + std::to_string(instaneous_unit_rate_);
        str += " pups=" + std::to_string(period_unit_rate_);
        str += Base::ToStringChild();
        return str;
    }
};
