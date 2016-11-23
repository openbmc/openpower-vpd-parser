#pragma once

#include <endian.h>

namespace endian
{
namespace details
{

template <typename T>
struct convert
{
    static T toHost(T) = delete;
    static T fromHost(T) = delete;
    static T toNetwork(T) = delete;
    static T fromNetwork(T) = delete;
};

template<> struct convert<uint16_t>
{
    static uint16_t toHost(uint16_t i) { return htole16(i); };
    static uint16_t fromHost(uint16_t i) { return le16toh(i); };
    static uint16_t toNetwork(uint16_t i) { return htobe16(i); };
    static uint16_t fromNetwork(uint16_t i) { return be16toh(i); };
};

} // namespace details

template<typename T> T toHost(T i)
{
    return details::convert<T>::toHost(i);
}

template<typename T> T fromHost(T i)
{
    return details::convert<T>::fromHost(i);
}

template<typename T> T toNetwork(T i)
{
    return details::convert<T>::toNetwork(i);
}

template<typename T> T fromNetwork(T i)
{
    return details::convert<T>::fromNetwork(i);
}

} // namespace endian
