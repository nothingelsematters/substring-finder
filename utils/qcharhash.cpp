#include <functional>
#include <QChar>


namespace std {
    template<> struct hash<QChar> {
        std::size_t operator()(QChar const& s) const noexcept {
            return std::hash<unsigned short>()(s.unicode());
        }
    };
}
