#include <iomanip>
#include <iostream>
#include <limits>
#include <ostream>
#include <string_view>
#include <type_traits>
#include <utility>

#if defined(__cpp_exceptions)
#include <stdexcept>
#endif


namespace ut::detail{


    template <typename T>
    [[nodiscard]] consteval std::string_view type_name(){
        #if defined(__GNUC__)
        return {&__PRETTY_FUNCTION__[61], sizeof(__PRETTY_FUNCTION__) - 112};
        #else
        #error Compiler not supported
        #endif
    }

    struct value_is_greater_then_types_max{
        value_is_greater_then_types_max(){}
    };

    template <std::integral T>
    [[nodiscard]] consteval T validating_cast(unsigned long long const v){
        if(v > std::numeric_limits<T>::max()){
            value_is_greater_then_types_max(); // call non-constexpr constructor
        }

        return static_cast<T>(v);
    }


    struct format{
        static constexpr auto pass = "\033[92m";
        static constexpr auto fail = "\033[91m";
        static constexpr auto ref = "\033[94m";
        static constexpr auto sum = "\033[93m";
        static constexpr auto reset = "\033[0m";

        static constexpr std::string_view color(bool const passed)noexcept{
            return passed ? pass : fail;
        }
    };


    struct constant_tag{};
    struct value_tag{};
    struct unary_tag{};
    struct binary_tag{};


    template <typename T>
    class constant: public constant_tag{
    public:
        using value_type = T;

        constexpr constant(T const& v)noexcept
            : value_(v)
            {}

        constant(constant const& other) = default;

        constant& operator=(constant const&) = delete;

        [[nodiscard]] constexpr auto operator==(auto const& other)const{
            return value_ == other;
        }

        [[nodiscard]] constexpr auto operator<=>(auto const& other)const{
            return value_ <=> other;
        }

        [[nodiscard]] constexpr T const& operator()()const noexcept{
            return value_;
        }

        friend std::ostream& operator<<(std::ostream& os, constant const& v){
            return os << v();
        }

    protected:
        T const value_;
    };


    template <typename T>
    struct arithmetic_constant: constant<T>{
        constexpr arithmetic_constant(T const& v)noexcept
            : constant<T>(v)
            {}

        [[nodiscard]] constexpr arithmetic_constant<T> operator+()const noexcept{
            return {+constant<T>::value_};
        }

        [[nodiscard]] constexpr arithmetic_constant<T> operator-()const noexcept{
            return {-constant<T>::value_};
        }
    };


}


namespace ut::concepts{


    template <typename T>
    concept arithmetic = std::is_arithmetic_v<T>;

    template <typename T>
    concept signed_arithmetic = arithmetic<T> && std::is_signed_v<T>;

    template <typename T>
    concept constant = std::derived_from<T, detail::constant_tag>;

    template <typename T>
    concept value = std::derived_from<T, detail::value_tag>;

    template <typename T>
    concept unary_expression = std::derived_from<T, detail::unary_tag>;

    template <typename T>
    concept binary_expression = std::derived_from<T, detail::binary_tag>;

    template <typename T>
    concept expression = value<T> || unary_expression<T> || binary_expression<T>;

    template <typename T>
    concept non_expression = !expression<T>;


}


namespace ut{


    using namespace std::literals;


    template <typename T>
    class basic_value: public detail::value_tag{
    public:
        using value_type = T;

        constexpr basic_value(T const& v)noexcept
            : value_(v)
            {}

        constexpr basic_value(basic_value const& other)noexcept
            : value_(other.value_)
            {}

        basic_value& operator=(basic_value const&) = delete;

        [[nodiscard]] constexpr T const& operator()()const noexcept{
            return value_;
        }

    protected:
        void print(
            std::ostream& os,
            std::string_view const color,
            bool const print_results,
            bool const print_types
        )const{
            static constexpr auto const type_name = []{
                if constexpr(concepts::constant<T>){
                    return detail::type_name<typename T::value_type>();
                }else{
                    return detail::type_name<T>();
                }
            }();

            if(print_types){
                os << "<" << type_name << ">(";
            }
            os << color;
            if constexpr(std::convertible_to<T const&, std::string_view const&>){
                os << std::quoted(static_cast<std::string_view const&>(value_));
            }else{
                os << value_;
            }
            os << detail::format::reset;
            if(print_types){
                os << ")";
            }
        }

    private:
        T const& value_;
    };

    template <typename T>
    struct value: basic_value<T>{
        using basic_value<T>::basic_value;

        [[nodiscard]] static consteval bool is_expected_only()noexcept{
            return false;
        }

        [[nodiscard]] static consteval bool all_conditions_true()noexcept{
            return true;
        }

        value(detail::constant<T> const& v) = delete;

        void print(std::ostream& os, bool const pass, bool const print_results, bool const print_types)const{
            basic_value<T>::print(os, detail::format::color(pass), print_results, print_types);
        }
    };

    template <typename T>
    struct expected: basic_value<T>{
        using basic_value<T>::basic_value;

        [[nodiscard]] static consteval bool is_expected_only()noexcept{
            return true;
        }

        [[nodiscard]] static consteval bool all_conditions_true()noexcept{
            return true;
        }

        constexpr expected(detail::constant<T> const& v)noexcept
            : basic_value<T>(v())
            {}

        void print(std::ostream& os, bool const pass, bool const print_results, bool const print_types)const{
            basic_value<T>::print(os, detail::format::ref, print_results, print_types);
        }
    };

    template <typename T>
    value(T&&) -> value<std::decay_t<T>>;

    template <typename T>
    expected(T&&) -> expected<std::decay_t<T>>;


}



namespace ut::detail{


    template <typename T>
    [[nodiscard]] auto to_expression(T const& v)noexcept{
        if constexpr(concepts::constant<T>){
            return expected<T>(v);
        }else if constexpr(concepts::non_expression<T>){
            return value<T>(v);
        }else{
            static_assert(concepts::expression<T>);
            return v;
        }
    }

    template <typename T>
    struct remove_ut_value{
        using type = T;
    };

    template <typename T>
    requires concepts::value<T> || concepts::constant<T>
    struct remove_ut_value<T>{
        using type = T::value_type;
    };

    template <typename T>
    using remove_ut_value_t = remove_ut_value<T>::type;


}


namespace ut{


    template <concepts::expression Expr>
    void expect(Expr const& expr){
        static_assert(!Expr::is_expected_only(), "Your condition dosn't contain any test values.");

        auto const flags(std::cout.flags());
        std::cout << std::boolalpha;
        auto const pass = expr.all_conditions_true();
        std::cout
            << detail::format::color(pass)
            << (pass ? "pass"sv : "fail"sv)
            << detail::format::reset << ": ";
        expr.print(std::cout, pass, false, false);
        std::cout << "\n";
        if(!pass){
            std::cout << "      "; expr.print(std::cout, pass, true, false); std::cout << "\n";
            std::cout << "      "; expr.print(std::cout, pass, false, true); std::cout << "\n";
            std::cout << "      "; expr.print(std::cout, pass, true, true);  std::cout << "\n";
        }
        std::cout.flags(flags);
    }


    template <concepts::expression A, concepts::expression B>
    class equal: public detail::binary_tag{
    public:
        equal(auto const& a, auto const& b)noexcept
            : a_(detail::to_expression(a))
            , b_(detail::to_expression(b))
            {}

        static consteval bool is_expected_only()noexcept{
            return A::is_expected_only() && B::is_expected_only();
        }

        [[nodiscard]] bool all_conditions_true()const{
            return a_.all_conditions_true() && b_.all_conditions_true() && (is_expected_only() ||
                [this]{
                    if constexpr(std::is_same_v<std::remove_cvref_t<decltype((*this)())>, bool>){
                        return (*this)();
                    }else{
                        return true;
                    }
                }());
        }

        [[nodiscard]] constexpr decltype(auto) operator()()const{
            return a_() == b_();
        }

        void print(std::ostream& os, bool const pass, bool const print_results, bool const print_types)const{
            bool const sub_pass =
                [this, pass]{
                    if constexpr(std::is_same_v<std::remove_cvref_t<decltype((*this)())>, bool>){
                        return (*this)();
                    }else{
                        return pass;
                    }
                }();
            os << "(";
            a_.print(os, sub_pass, print_results, print_types);
            os << (sub_pass ? detail::format::pass : detail::format::fail) << " == " << detail::format::reset;
            b_.print(os, sub_pass, print_results, print_types);
            if(print_results){
                os << " => ";
                auto const to_expression =
                    [](concepts::non_expression auto const& v){
                        if constexpr(is_expected_only()){
                            return expected(v);
                        }else{
                            return value(v);
                        }
                    };

                to_expression((*this)()).print(os, sub_pass, print_results, print_types);
            }
            os << ")";
        }

    private:
        A const a_;
        B const b_;
    };

    equal(auto const& a, auto const& b)
        -> equal<decltype(detail::to_expression(a)), decltype(detail::to_expression(b))>;


}


namespace ut::inline literals{


    [[nodiscard]] consteval auto operator""_i(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<int>(v)};
    }

    [[nodiscard]] consteval auto operator""_s(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<short>(v)};
    }

    [[nodiscard]] consteval auto operator""_c(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<char>(v)};
    }

    [[nodiscard]] consteval auto operator""_sc(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<signed char>(v)};
    }

    [[nodiscard]] consteval auto operator""_l(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<long>(v)};
    }

    [[nodiscard]] consteval auto operator""_ll(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<long long>(v)};
    }

    [[nodiscard]] consteval auto operator""_u(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<unsigned>(v)};
    }

    [[nodiscard]] consteval auto operator""_uc(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<unsigned char>(v)};
    }

    [[nodiscard]] consteval auto operator""_us(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<unsigned short>(v)};
    }

    [[nodiscard]] consteval auto operator""_ul(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<unsigned long>(v)};
    }

    [[nodiscard]] consteval auto operator""_ull(unsigned long long const v) {
        return detail::arithmetic_constant{v};
    }

    [[nodiscard]] consteval auto operator""_i8(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<std::int8_t>(v)};
    }

    [[nodiscard]] consteval auto operator""_i16(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<std::int16_t>(v)};
    }

    [[nodiscard]] consteval auto operator""_i32(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<std::int32_t>(v)};
    }

    [[nodiscard]] consteval auto operator""_i64(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<std::int64_t>(v)};
    }

    [[nodiscard]] consteval auto operator""_u8(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<std::uint8_t>(v)};
    }

    [[nodiscard]] consteval auto operator""_u16(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<std::uint16_t>(v)};
    }

    [[nodiscard]] consteval auto operator""_u32(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<std::uint32_t>(v)};
    }

    [[nodiscard]] consteval auto operator""_u64(unsigned long long const v) {
        return detail::arithmetic_constant{detail::validating_cast<std::uint64_t>(v)};
    }

    [[nodiscard]] constexpr auto operator""_f(long double const v) {
        return detail::arithmetic_constant{static_cast<float>(v)};
    }

    [[nodiscard]] constexpr auto operator""_d(long double const v) {
        return detail::arithmetic_constant{static_cast<double>(v)};
    }

    [[nodiscard]] constexpr auto operator""_ld(long double const v) {
        return detail::arithmetic_constant{v};
    }


}


namespace ut::operators{


    auto operator==(concepts::expression auto const& a, concepts::expression auto const& b)noexcept{
        return equal(a, b);
    }


}


#include <string>

int main(){
    using namespace std::literals;
    using namespace ut::literals;
    using namespace ut::operators;

    ut::expect(ut::equal(0_i, 0));
    ut::expect(ut::equal(1.5_f, 1.5));
    ut::expect(ut::equal(+1.5_f, 1.5));
    ut::expect(ut::equal(-1.5_f, 1.5));
    ut::expect(ut::equal(3_i, 3));
    ut::expect(ut::equal(+3_i, 3));
    ut::expect(ut::equal(-3_i, 3));
    ut::expect(ut::equal(6_u64, 6));
    ut::expect(ut::value("tested"sv) == ut::expected("tested"sv));
    ut::expect(ut::value("tested"sv) == ut::value("tested"sv));
    ut::expect(ut::expected("reference"sv) == ut::value("tested"sv) == ut::value(true));
    ut::expect(ut::equal("tested"sv, "tested"sv) == ut::value(false));
    ut::expect(ut::expected("reference"sv) == ut::value("tested"sv) == ut::value(false));
    ut::expect(ut::expected("reference"sv) == ut::expected("tested"sv) == ut::value(false));
    ut::expect(ut::expected(1) == ut::value(2));
}